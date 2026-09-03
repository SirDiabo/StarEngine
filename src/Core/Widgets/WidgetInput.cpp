#include "WidgetInput.h"
#include "Widget.h"
#include "WidgetElement.h"
#include "Core/InputMappingRegistry.h"
#include "Core/InputKeyNames.h"
#include <vector>
#include <cmath>
#include <unordered_map>

namespace {

std::unordered_map<std::string, Widget*> focusedWidgetByDevice;

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
            {"W", 1.0f, 1}, {"S", -1.0f, 1}, {"A", -1.0f, 0}, {"D", 1.0f, 0},
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
    if (device == "Unclaimed") {
        if (InputKeyBelongsToDevice(key, "Keyboard+Mouse", gamepadIndex)) return true;
        gamepadIndex = 0;
        return InputKeyBelongsToDevice(key, "Gamepad 0", gamepadIndex);
    }
    return InputKeyBelongsToDevice(key, device, gamepadIndex);
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

Vector2 PollAxis2D(const char* mappingName, const std::string& device) {
    Vector2 result{0.0f, 0.0f};
    const InputMapping* mapping = InputMappingRegistry::Get().Find(mappingName);
    if (!mapping) return result;
    for (const InputMappingBinding& binding : mapping->bindings) {
        int gamepadIndex = 0;
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

void SyncStates(WidgetElement* element, WidgetElement* current, WidgetElement* pressed) {
    if (element->selectable) {
        if (element->disabled) element->interactionState = WidgetInteractionState::Disabled;
        else if (element == pressed) element->interactionState = WidgetInteractionState::Pressed;
        else if (element == current) element->interactionState = WidgetInteractionState::Selected;
        else element->interactionState = WidgetInteractionState::Default;
    }
    for (WidgetElement* child : element->GetChildren()) SyncStates(child, current, pressed);
}

}

namespace WidgetInput {

void ClaimFocus(Widget* widget, const std::string& device) {
    focusedWidgetByDevice[device] = widget;
}

bool IsFocused(const Widget* widget, const std::string& device) {
    auto it = focusedWidgetByDevice.find(device);
    if (it == focusedWidgetByDevice.end()) return true;
    return it->second == widget;
}

void ProcessInput(Widget* widget, const std::string& device, float deltaTime, Vector2 localMousePos) {
    if (!widget || !widget->rootElement) return;
    EnsureDefaultMappings();

    bool focused = IsFocused(widget, device);

    std::vector<WidgetElement*> selectables;
    widget->rootElement->CollectSelectables(selectables);

    if (!widget->currentSelection && !selectables.empty()) {
        widget->SetSelection(selectables.front());
    }

    WidgetElement* hovered = focused ? HitTestSelectable(widget->rootElement, localMousePos) : nullptr;
    if (hovered) widget->SetSelection(hovered);

    bool confirmHeld = focused && PollBool(kConfirmMapping, device);
    bool cancelHeld = focused && PollBool(kCancelMapping, device);
    bool mouseHeld = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (focused && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        hovered->OnConfirmed();
    }

    int currentIndex = -1;
    for (int i = 0; i < (int)selectables.size(); i++) {
        if (selectables[i] == widget->currentSelection) { currentIndex = i; break; }
    }
    if (currentIndex < 0 && !selectables.empty()) {
        widget->SetSelection(selectables.front());
        currentIndex = 0;
    }

    if (widget->navigateCooldown > 0.0f) widget->navigateCooldown -= deltaTime;

    if (focused && currentIndex >= 0 && widget->navigateCooldown <= 0.0f) {
        Vector2 nav = PollAxis2D(kNavigateMapping, device);
        int dx = 0, dy = 0;
        if (fabsf(nav.y) > fabsf(nav.x)) dy = nav.y > 0.4f ? -1 : (nav.y < -0.4f ? 1 : 0);
        else dx = nav.x > 0.4f ? 1 : (nav.x < -0.4f ? -1 : 0);

        if (dx != 0 || dy != 0) {
            WidgetElement* current = widget->currentSelection;
            WidgetElement* next = nullptr;
            if (current && current->parent) {
                next = current->parent->GetNextSelectableInDirection(current, dx, dy);
            }
            if (!next) {
                int step = (dy != 0) ? dy : dx;
                int nextIndex = currentIndex + step;
                if (nextIndex >= 0 && nextIndex < (int)selectables.size()) {
                    next = selectables[nextIndex];
                }
            }
            if (next) widget->SetSelection(next);
            widget->navigateCooldown = 0.25f;
        }
    }

    if (confirmHeld && !widget->confirmWasHeld && widget->currentSelection) {
        widget->currentSelection->OnConfirmed();
    }
    widget->confirmWasHeld = confirmHeld;
    widget->cancelWasHeld = cancelHeld;

    WidgetElement* pressedElement = nullptr;
    if (mouseHeld) pressedElement = hovered;
    else if (confirmHeld) pressedElement = widget->currentSelection;
    SyncStates(widget->rootElement, focused ? widget->currentSelection : nullptr, pressedElement);
}

}
