#include "WidgetInput.h"
#include "TextElement.h"
#include "Widget.h"
#include "WidgetElement.h"
#include "BoxElement.h"
#include "PointerEvents.h"
#include "Core/InputMappingRegistry.h"
#include "Core/InputKeyNames.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace {

bool TreeContainsElement(const WidgetElement* root, const WidgetElement* target) {
    if (!root || !target) return false;
    if (root == target) return true;
    for (const WidgetElement* child : root->GetChildren()) {
        if (TreeContainsElement(child, target)) return true;
    }
    return false;
}

struct FocusState {
    const void* owner = nullptr;
    Widget* widget = nullptr;
};

struct WidgetEntry {
    Widget* widget = nullptr;
    const void* owner = nullptr;
    double lastSeen = 0.0;
    unsigned long long sequence = 0;
};

const double kLiveWindow = 0.25;
const double kCaptureWindow = 0.15;

std::unordered_map<std::string, FocusState> focusByDevice;
std::unordered_map<std::string, std::vector<const void*>> modalsByDevice;
std::unordered_map<const Widget*, WidgetEntry> entries;
std::unordered_map<const void*, Widget*> lastWidgetByOwner;

struct Occluder {
    Rectangle rect;
    const void* owner;
};

std::vector<Occluder> occluders;
std::vector<Occluder> occludersNext;

const void* inputScope = nullptr;
unsigned long long nextSequence = 0;
double lastCaptureTime = -1000.0;

const char* kNavigateMapping = "UI_Navigate";
const char* kConfirmMapping = "UI_Confirm";
const char* kCancelMapping = "UI_Cancel";

void EnsureDefaultMappings() {
    static bool seeded = false;
    if (seeded) return;
    seeded = true;

    bool addedAny = false;

    if (!InputMappingRegistry::Get().Find(kNavigateMapping)) {
        InputMapping navigate;
        navigate.type = InputMappingType::Axis2D;
        navigate.bindings = {
            {"UP", 1.0f, 1}, {"DOWN", -1.0f, 1}, {"LEFT", -1.0f, 0}, {"RIGHT", 1.0f, 0},
            {"GAMEPAD_LEFT_STICK_X", 1.0f, 0}, {"GAMEPAD_LEFT_STICK_Y", -1.0f, 1},
            {"GAMEPAD_DPAD_UP", 1.0f, 1}, {"GAMEPAD_DPAD_DOWN", -1.0f, 1},
            {"GAMEPAD_DPAD_LEFT", -1.0f, 0}, {"GAMEPAD_DPAD_RIGHT", 1.0f, 0}
        };
        InputMappingRegistry::Get().SetMapping(kNavigateMapping, navigate);
        addedAny = true;
    }
    if (!InputMappingRegistry::Get().Find(kConfirmMapping)) {
        InputMapping confirm;
        confirm.type = InputMappingType::Bool;
        confirm.bindings = {{"ENTER"}, {"SPACE"}, {"GAMEPAD_FACE_DOWN"}};
        InputMappingRegistry::Get().SetMapping(kConfirmMapping, confirm);
        addedAny = true;
    }
    if (!InputMappingRegistry::Get().Find(kCancelMapping)) {
        InputMapping cancel;
        cancel.type = InputMappingType::Bool;
        cancel.bindings = {{"ESCAPE"}, {"GAMEPAD_FACE_RIGHT"}};
        InputMappingRegistry::Get().SetMapping(kCancelMapping, cancel);
        addedAny = true;
    }

    if (addedAny) InputMappingRegistry::Get().Save();
}

bool KeyMatchesDevice(const std::string& key, const std::string& device, int& gamepadIndex) {
    if (device == "Keyboard+Mouse" || device.rfind("Gamepad ", 0) == 0) {
        return InputKeyBelongsToDevice(key, device, gamepadIndex);
    }
    if (InputKeyBelongsToDevice(key, "Keyboard+Mouse", gamepadIndex)) return true;
    gamepadIndex = 0;
    return InputKeyBelongsToDevice(key, "Gamepad 0", gamepadIndex);
}

bool PollBool(const char* mappingName, const std::string& device) {
    const InputMapping* mapping = InputMappingRegistry::Get().Find(mappingName);
    if (!mapping) return false;
    for (const InputMappingBinding& binding : mapping->bindings) {
        int gamepadIndex = 0;
        if (!KeyMatchesDevice(binding.key, device, gamepadIndex)) continue;
        float raw = 0.0f;
        if (PollInputKey(binding.key, raw, gamepadIndex) && raw != 0.0f) return true;
    }
    return false;
}

bool IsArrowOrPadBinding(const std::string& key) {
    return key == "UP" || key == "DOWN" || key == "LEFT" || key == "RIGHT" || key.rfind("GAMEPAD", 0) == 0;
}

Vector2 PollAxis2D(const char* mappingName, const std::string& device, bool arrowsOnly = false) {
    Vector2 result{0.0f, 0.0f};
    const InputMapping* mapping = InputMappingRegistry::Get().Find(mappingName);
    if (!mapping) return result;
    for (const InputMappingBinding& binding : mapping->bindings) {
        int gamepadIndex = 0;
        if (arrowsOnly && !IsArrowOrPadBinding(binding.key)) continue;
        if (!KeyMatchesDevice(binding.key, device, gamepadIndex)) continue;
        float raw = 0.0f;
        if (!PollInputKey(binding.key, raw, gamepadIndex)) continue;
        if (binding.axis == 0) result.x += raw * binding.scale;
        else result.y += raw * binding.scale;
    }
    return result;
}

WidgetElement* HitTestSelectable(WidgetElement* element, Vector2 point) {
    if (!element->visible) return nullptr;
    for (int i = (int)element->GetChildren().size() - 1; i >= 0; i--) {
        WidgetElement* childHit = HitTestSelectable(element->GetChildren()[i], point);
        if (childHit) return childHit;
    }
    if (element->selectable && !element->disabled && CheckCollisionPointRec(point, element->lastResolvedRect)) {
        return element;
    }
    return nullptr;
}

void SyncStates(WidgetElement* element, WidgetElement* current, WidgetElement* hoveredEl, WidgetElement* pressed) {
    if (element->selectable) {
        if (element->disabled) element->interactionState = WidgetInteractionState::Disabled;
        else if (element == pressed) element->interactionState = WidgetInteractionState::Pressed;
        else if (element == hoveredEl) element->interactionState = WidgetInteractionState::Hovered;
        else if (element == current) element->interactionState = WidgetInteractionState::Selected;
        else element->interactionState = WidgetInteractionState::Default;
    }
    for (WidgetElement* child : element->GetChildren()) SyncStates(child, current, hoveredEl, pressed);
}

BoxElement* HitTestScrollable(WidgetElement* element, Vector2 point) {
    if (!element->visible) return nullptr;
    for (int i = (int)element->GetChildren().size() - 1; i >= 0; i--) {
        BoxElement* childHit = HitTestScrollable(element->GetChildren()[i], point);
        if (childHit) return childHit;
    }
    BoxElement* box = dynamic_cast<BoxElement*>(element);
    if (box && box->scrollEnabled && CheckCollisionPointRec(point, element->lastResolvedRect)) {
        return box;
    }
    return nullptr;
}


bool IsManagedDevice(const std::string& device) {
    return device == WidgetInput::kEditorDevice;
}

bool IsLive(const WidgetEntry& entry) {
    return GetTime() - entry.lastSeen <= kLiveWindow;
}

const void* TopModal(const std::string& device) {
    auto it = modalsByDevice.find(device);
    if (it == modalsByDevice.end() || it->second.empty()) return nullptr;
    return it->second.back();
}

const void* OwnerOf(const Widget* widget) {
    auto it = entries.find(widget);
    if (it != entries.end()) return it->second.owner;
    return static_cast<const void*>(widget);
}

Widget* ResolveWidgetForOwner(const void* owner) {
    auto last = lastWidgetByOwner.find(owner);
    if (last != lastWidgetByOwner.end()) {
        auto e = entries.find(last->second);
        if (e != entries.end() && e->second.owner == owner && IsLive(e->second)) return last->second;
    }
    Widget* best = nullptr;
    unsigned long long bestSequence = ~0ull;
    for (auto& kv : entries) {
        const WidgetEntry& e = kv.second;
        if (e.owner != owner || !IsLive(e)) continue;
        if (e.sequence < bestSequence) {
            bestSequence = e.sequence;
            best = e.widget;
        }
    }
    return best;
}

void ClaimFocusInternal(Widget* widget, const void* owner, const std::string& device) {
    FocusState& state = focusByDevice[device];
    state.owner = owner;
    state.widget = widget;
    if (widget) lastWidgetByOwner[owner] = widget;
}

bool IsFocusedInternal(const Widget* widget, const void* owner, const std::string& device) {
    if (const void* modal = TopModal(device)) return modal == owner;
    auto it = focusByDevice.find(device);
    if (it == focusByDevice.end()) return true;
    return it->second.widget == widget;
}

void BeginEditIfText(WidgetElement* element) {
    if (TextElement* text = dynamic_cast<TextElement*>(element)) text->StartEditingFromNavigation();
}

void RestoreSelection(Widget* widget, std::vector<WidgetElement*>& selectables) {
    if (widget->currentSelection) return;
    selectables.clear();
    widget->rootElement->CollectSelectables(selectables);
    if (selectables.empty()) return;
    WidgetElement* pick = nullptr;
    if (!widget->pendingSelectionKey.empty()) {
        for (WidgetElement* candidate : selectables) {
            if (candidate->selectionKey == widget->pendingSelectionKey) { pick = candidate; break; }
        }
    }
    widget->pendingSelectionKey.clear();
    widget->SetSelection(pick ? pick : selectables.front());
}

bool MoveSelectionSpatially(Widget* from, WidgetElement* current, int dx, int dy, const std::string& device) {
    const void* modalOwner = TopModal(device);
    Rectangle cur = current->lastResolvedRect;
    Vector2 curCenter = { cur.x + cur.width * 0.5f, cur.y + cur.height * 0.5f };

    Widget* bestWidget = nullptr;
    WidgetElement* bestElement = nullptr;
    const void* bestOwner = nullptr;
    float bestScore = 1e30f;

    for (auto& kv : entries) {
        const WidgetEntry& entry = kv.second;
        if (!IsLive(entry)) continue;
        if (modalOwner && modalOwner != entry.owner) continue;
        Widget* candidateWidget = entry.widget;
        if (!candidateWidget || !candidateWidget->rootElement || !candidateWidget->rootElement->visible) continue;

        Rectangle clip = candidateWidget->rootElement->lastResolvedRect;
        std::vector<WidgetElement*> candidates;
        candidateWidget->rootElement->CollectSelectables(candidates);

        for (WidgetElement* candidate : candidates) {
            if (candidate == current) continue;
            Rectangle r = candidate->lastResolvedRect;
            if (r.width <= 0.0f || r.height <= 0.0f) continue;
            Vector2 center = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
            if (!CheckCollisionPointRec(center, clip)) continue;

            float gap;
            if (dx > 0) gap = r.x - (cur.x + cur.width);
            else if (dx < 0) gap = cur.x - (r.x + r.width);
            else if (dy > 0) gap = r.y - (cur.y + cur.height);
            else gap = cur.y - (r.y + r.height);
            if (gap < -2.0f) continue;

            float along = (center.x - curCenter.x) * (float)dx + (center.y - curCenter.y) * (float)dy;
            if (along <= 0.0f) continue;
            float perpendicular = (dx != 0) ? fabsf(center.y - curCenter.y) : fabsf(center.x - curCenter.x);

            float score = along + perpendicular * 2.0f;
            if (score < bestScore) {
                bestScore = score;
                bestWidget = candidateWidget;
                bestElement = candidate;
                bestOwner = entry.owner;
            }
        }
    }

    if (!bestElement) return false;

    if (bestWidget == from) {
        from->SetSelection(bestElement);
    } else {
        if (TextElement* leaving = dynamic_cast<TextElement*>(current)) {
            if (leaving->isEditing) leaving->OnDeselected();
        }
        ClaimFocusInternal(bestWidget, bestOwner, device);
        bestWidget->SetSelection(bestElement);
        bestWidget->navigateCooldown = 0.25f;
    }
    BeginEditIfText(bestElement);
    return true;
}

}

namespace WidgetInput {

void BeginFrame() {
    occluders.swap(occludersNext);
    occludersNext.clear();
}

void SetInputScope(const void* owner) {
    inputScope = owner;
}

const void* GetInputScope() {
    return inputScope;
}

void AddOccluder(Rectangle rect, const void* owner) {
    occludersNext.push_back({ rect, owner });
}

bool IsMouseOccluded(Vector2 position, const void* owner) {
    for (const Occluder& occluder : occluders) {
        if (occluder.owner != owner && CheckCollisionPointRec(position, occluder.rect)) return true;
    }
    return false;
}

bool IsMouseBlocked() {
    const void* modal = TopModal(kEditorDevice);
    if (modal && modal != inputScope) return true;
    return IsMouseOccluded(GetMousePosition(), inputScope);
}

void ClaimFocus(Widget* widget, const std::string& device) {
    if (!widget) return;
    ClaimFocusInternal(widget, OwnerOf(widget), device);
}

void ClaimOwnerFocus(const void* owner, const std::string& device) {
    FocusState& state = focusByDevice[device];
    if (state.owner == owner) return;
    state.owner = owner;
    state.widget = ResolveWidgetForOwner(owner);
}

void ReleaseFocus(const std::string& device) {
    focusByDevice.erase(device);
}

bool IsFocused(const Widget* widget, const std::string& device) {
    return IsFocusedInternal(widget, OwnerOf(widget), device);
}

const void* GetFocusOwner(const std::string& device) {
    auto it = focusByDevice.find(device);
    if (it == focusByDevice.end()) return nullptr;
    return it->second.owner;
}

void PushModal(const void* owner, const std::string& device) {
    std::vector<const void*>& stack = modalsByDevice[device];
    if (std::find(stack.begin(), stack.end(), owner) == stack.end()) stack.push_back(owner);
}

void PopModal(const void* owner, const std::string& device) {
    auto it = modalsByDevice.find(device);
    if (it == modalsByDevice.end()) return;
    std::vector<const void*>& stack = it->second;
    stack.erase(std::remove(stack.begin(), stack.end(), owner), stack.end());
}

bool HasModal(const std::string& device) {
    return TopModal(device) != nullptr;
}

void MarkKeyboardCaptured() {
    lastCaptureTime = GetTime();
}

bool IsKeyboardCaptured() {
    return GetTime() - lastCaptureTime <= kCaptureWindow;
}

bool ScopeHasKeyboard() {
    if (IsKeyboardCaptured()) return false;
    if (!inputScope) return true;
    if (const void* modal = TopModal(kEditorDevice)) return modal == inputScope;
    auto it = focusByDevice.find(kEditorDevice);
    if (it == focusByDevice.end()) return true;
    return it->second.owner == inputScope;
}

bool WidgetHasKeyboard(const Widget* widget) {
    if (!widget || IsKeyboardCaptured()) return false;
    const void* owner = OwnerOf(widget);
    const void* modal = TopModal(kEditorDevice);
    if (modal && modal != owner) return false;
    return IsFocusedInternal(widget, owner, kEditorDevice);
}

void Forget(const Widget* widget) {
    entries.erase(widget);
    for (auto it = lastWidgetByOwner.begin(); it != lastWidgetByOwner.end();) {
        if (it->second == widget) it = lastWidgetByOwner.erase(it);
        else ++it;
    }
    for (auto& kv : focusByDevice) {
        if (kv.second.widget == widget) kv.second.widget = nullptr;
    }
    for (auto& kv : modalsByDevice) {
        std::vector<const void*>& stack = kv.second;
        stack.erase(std::remove(stack.begin(), stack.end(), static_cast<const void*>(widget)), stack.end());
    }
}

void ProcessInput(Widget* widget, const std::string& device, float deltaTime, Vector2 localMousePos, float uiScale) {
    if (!widget || !widget->rootElement) return;
    EnsureDefaultMappings();

    const bool managed = IsManagedDevice(device);
    const void* owner = inputScope ? inputScope : static_cast<const void*>(widget);
    const Rectangle rootRect = widget->rootElement->lastResolvedRect;

    if (managed) {
        WidgetEntry& entry = entries[widget];
        if (entry.sequence == 0) entry.sequence = ++nextSequence;
        entry.widget = widget;
        entry.owner = owner;
        entry.lastSeen = GetTime();

        auto focusIt = focusByDevice.find(device);
        if (focusIt != focusByDevice.end() && focusIt->second.owner == owner && !focusIt->second.widget) {
            focusIt->second.widget = widget;
            lastWidgetByOwner[owner] = widget;
        }
    }

    const void* modalOwner = TopModal(device);
    const bool blocked = modalOwner && modalOwner != owner;
    const bool occluded = managed && IsMouseOccluded(localMousePos, owner);
    const bool mouseActive = !blocked && !occluded;

    const bool anyPress = IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)
        || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    if (managed && mouseActive && !modalOwner && anyPress && CheckCollisionPointRec(localMousePos, rootRect)) {
        ClaimFocusInternal(widget, owner, device);
    }

    const bool focused = !blocked && IsFocusedInternal(widget, owner, device);

    float wheel = GetMouseWheelMove();
    if (mouseActive && wheel != 0.0f) {
        BoxElement* scrollTarget = HitTestScrollable(widget->rootElement, localMousePos);
        if (scrollTarget) {
            float scale = (uiScale > 0.0f) ? uiScale : 1.0f;
            float contentExtent = scrollTarget->GetContentExtent(1.0f);
            float containerExtent = (scrollTarget->axis == BoxAxis::Vertical
                                          ? scrollTarget->lastResolvedRect.height
                                          : scrollTarget->lastResolvedRect.width) / scale;
            float maxScroll = std::max(0.0f, contentExtent - containerExtent);
            scrollTarget->scrollOffset -= wheel * 40.0f;
            if (scrollTarget->scrollOffset < 0.0f) scrollTarget->scrollOffset = 0.0f;
            if (scrollTarget->scrollOffset > maxScroll) scrollTarget->scrollOffset = maxScroll;
        }
    }

    std::vector<WidgetElement*> selectables;
    widget->rootElement->CollectSelectables(selectables);

    RestoreSelection(widget, selectables);

    WidgetElement* hovered = mouseActive ? HitTestSelectable(widget->rootElement, localMousePos) : nullptr;

    if (mouseActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered || CheckCollisionPointRec(localMousePos, rootRect) || IsKeyboardCaptured()) {
            widget->SetSelection(hovered);
        }
    }

    const bool justFocused = focused && !widget->hadKeyboardFocus;
    widget->hadKeyboardFocus = focused;

    const bool confirmRaw = focused && PollBool(kConfirmMapping, device);
    const bool cancelRaw = focused && PollBool(kCancelMapping, device);
    const bool keyActive = focused && !justFocused && !IsKeyboardCaptured();
    TextElement* editingText = dynamic_cast<TextElement*>(widget->currentSelection);
    if (editingText && !editingText->isEditing) editingText = nullptr;
    const bool editingNav = focused && !justFocused && editingText != nullptr;
    const int edgeRequest = editingNav ? editingText->ConsumeEdgeNavRequest() : 0;
    WidgetElement* previousPressOrigin = widget->pressOrigin;
    WidgetElement* previousHovered = widget->lastHoveredElement;
    const bool pressedThisFrame = mouseActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (pressedThisFrame) widget->pressOrigin = hovered;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) widget->pressOrigin = nullptr;
    widget->lastHoveredElement = hovered;

    {
        std::vector<PointerEvents::Event<WidgetElement>> pointerEvents;
        PointerEvents::Compute<WidgetElement>(previousHovered, previousPressOrigin, hovered, pressedThisFrame,
                                              IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                              [widget](WidgetElement* e) { return TreeContainsElement(widget->rootElement, e); },
                                              pointerEvents);
        for (const PointerEvents::Event<WidgetElement>& ev : pointerEvents) {
            if (ev.element->RaisesPointerEvents()) widget->FireElementEvent(ev.element, ev.name);
        }
    }
    bool mouseHeld = hovered && widget->pressOrigin == hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (mouseActive && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hovered->OnConfirmed();
    }

    if (mouseActive && hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (hovered->onRightActivated) hovered->onRightActivated();
    }

    if (!widget->currentSelection) RestoreSelection(widget, selectables);

    int currentIndex = -1;
    for (int i = 0; i < (int)selectables.size(); i++) {
        if (selectables[i] == widget->currentSelection) { currentIndex = i; break; }
    }
    if (currentIndex < 0 && !selectables.empty()) {
        widget->SetSelection(selectables.front());
        currentIndex = 0;
    }

    if (widget->navigateCooldown > 0.0f) widget->navigateCooldown -= deltaTime;

    if ((keyActive || editingNav) && currentIndex >= 0 && widget->navigateCooldown <= 0.0f) {
        Vector2 nav = PollAxis2D(kNavigateMapping, device, editingNav);
        int dx = 0, dy = 0;
        if (editingNav) {
            dy = nav.y > 0.4f ? -1 : (nav.y < -0.4f ? 1 : 0);
            if (dy == 0) dx = edgeRequest;
        } else if (fabsf(nav.y) > fabsf(nav.x)) {
            dy = nav.y > 0.4f ? -1 : (nav.y < -0.4f ? 1 : 0);
        } else {
            dx = nav.x > 0.4f ? 1 : (nav.x < -0.4f ? -1 : 0);
        }

        if (dx != 0 || dy != 0) {
            WidgetElement* current = widget->currentSelection;
            WidgetElement* next = nullptr;
            if (current && current->parent) {
                next = current->parent->GetNextSelectableInDirection(current, dx, dy);
            }
            bool movedSpatially = false;
            if (!next && current && managed) {
                movedSpatially = MoveSelectionSpatially(widget, current, dx, dy, device);
            }
            if (!next && !movedSpatially) {
                int step = (dy != 0) ? dy : dx;
                int nextIndex = currentIndex + step;
                if (nextIndex >= 0 && nextIndex < (int)selectables.size()) {
                    next = selectables[nextIndex];
                }
            }
            if (next) {
                widget->SetSelection(next);
                BeginEditIfText(next);
            }
            widget->navigateCooldown = 0.25f;
        }
    }

    if (keyActive && confirmRaw && !widget->confirmWasHeld && widget->currentSelection) {
        WidgetElement* target = widget->currentSelection;
        if (target->onKeyActivated) {
            target->onKeyActivated();
        } else {
            target->OnConfirmed();
            if (target->RaisesPointerEvents()) {
                widget->FireElementEvent(target, "Pressed");
                widget->FireElementEvent(target, "Released");
                widget->FireElementEvent(target, "Clicked");
            }
        }
    }
    widget->confirmWasHeld = confirmRaw;
    widget->cancelWasHeld = cancelRaw;

    WidgetElement* pressedElement = nullptr;
    if (mouseHeld) pressedElement = hovered;
    else if (keyActive && confirmRaw) pressedElement = widget->currentSelection;
    SyncStates(widget->rootElement, focused ? widget->currentSelection : nullptr, hovered, pressedElement);
}

}
