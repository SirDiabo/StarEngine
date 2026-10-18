#include "WidgetObjectElement.h"
#include "SlotElement.h"
#include "Widget.h"
#include "WidgetAsset.h"
#include "Runtime/WidgetBehavior.h"
#include "raylib.h"
#include <algorithm>

namespace {

bool behaviorsEnabled = true;
int buildDepth = 0;
const int kMaxNestingDepth = 8;

bool TreeContains(const WidgetElement* root, const WidgetElement* target) {
    if (!root || !target) return false;
    if (root == target) return true;
    for (const WidgetElement* child : root->GetChildren()) {
        if (TreeContains(child, target)) return true;
    }
    return false;
}

void FindSlots(WidgetElement* node, std::vector<SlotElement*>& out) {
    if (!node) return;
    if (SlotElement* slot = dynamic_cast<SlotElement*>(node)) { out.push_back(slot); return; }
    if (dynamic_cast<WidgetObjectElement*>(node)) return;
    for (WidgetElement* child : node->GetChildren()) FindSlots(child, out);
}

}

bool WidgetObjectElement::BehaviorsEnabled() { return behaviorsEnabled; }

WidgetObjectElement::BehaviorScope::BehaviorScope(bool enabled) : previous(behaviorsEnabled) { behaviorsEnabled = enabled; }
WidgetObjectElement::BehaviorScope::~BehaviorScope() { behaviorsEnabled = previous; }

WidgetObjectElement::~WidgetObjectElement() {
    Teardown();
    for (WidgetElement* fill : unplaced) delete fill;
    unplaced.clear();
}

void WidgetObjectElement::CollectSlots(std::vector<SlotElement*>& out) const {
    if (childWidget && !children.empty()) FindSlots(children[0], out);
}

std::vector<std::string> WidgetObjectElement::GetSlotNames() const {
    std::vector<SlotElement*> slots;
    CollectSlots(slots);
    std::vector<std::string> names;
    for (SlotElement* slot : slots) names.push_back(slot->slotName);
    return names;
}

void WidgetObjectElement::SetWidgetAsset(const std::string& path) {
    if (path == widgetAssetPath && (childWidget || path.empty())) return;
    widgetAssetPath = path;
    Build();
    if (childWidget && !children.empty() && size.x == 100.0f && size.y == 30.0f) {
        size = children[0]->size;
    }
}

void WidgetObjectElement::Teardown() {
    if (!childWidget) return;

    std::vector<SlotElement*> slots;
    CollectSlots(slots);
    std::vector<WidgetElement*> fills;
    for (SlotElement* slot : slots) {
        for (WidgetElement* fill : slot->GetChildren()) fills.push_back(fill);
    }
    for (WidgetElement* fill : fills) {
        if (fill->parent) fill->parent->DetachChild(fill);
        fill->parent = this;
        unplaced.push_back(fill);
    }

    if (owningWidget && owningWidget->currentSelection && !children.empty() &&
        TreeContains(children[0], owningWidget->currentSelection)) {
        owningWidget->pendingSelectionKey = owningWidget->currentSelection->selectionKey;
        owningWidget->currentSelection = nullptr;
    }

    children.clear();
    delete childWidget;
    childWidget = nullptr;
}

void WidgetObjectElement::Build() {
    Teardown();
    if (widgetAssetPath.empty()) return;
    if (buildDepth >= kMaxNestingDepth) {
        TraceLog(LOG_WARNING, "WidgetObjectElement: nesting deeper than %d levels at \"%s\"; possible self-reference",
                 kMaxNestingDepth, widgetAssetPath.c_str());
        return;
    }

    buildDepth++;
    Widget* widget = WidgetAsset::InstantiateWidget(widgetAssetPath, false);
    buildDepth--;
    if (!widget) return;
    if (!widget->rootElement) { delete widget; return; }

    WidgetElement* root = widget->rootElement;
    root->parent = this;
    children.push_back(root);
    childWidget = widget;

    std::vector<WidgetElement*> pending = std::move(unplaced);
    unplaced.clear();
    for (WidgetElement* fill : pending) {
        if (!PlaceFill(fill)) unplaced.push_back(fill);
    }

    if (owningWidget) childWidget->owningComponent = owningWidget->owningComponent;
    if (behaviorsEnabled) WidgetAsset::AttachWidgetBehavior(childWidget, widgetAssetPath);
}

bool WidgetObjectElement::PlaceFill(WidgetElement* fill) {
    if (!fill) return false;
    std::vector<SlotElement*> slots;
    CollectSlots(slots);
    SlotElement* target = nullptr;
    for (SlotElement* slot : slots) {
        bool nameMatches = fill->slotName.empty() || slot->slotName == fill->slotName;
        if (nameMatches && slot->GetChildren().empty()) { target = slot; break; }
    }
    if (!target) return false;
    target->AddChild(fill);
    fill->slotName = target->slotName;
    fill->SetOwningWidgetRecursive(owningWidget);
    return true;
}

bool WidgetObjectElement::SetFillSlot(WidgetElement* fill, const std::string& slotName) {
    if (!fill || slotName.empty()) return false;
    std::vector<SlotElement*> slots;
    CollectSlots(slots);
    for (SlotElement* slot : slots) {
        if (slot->slotName != slotName || !slot->GetChildren().empty()) continue;
        if (fill->parent) fill->parent->DetachChild(fill);
        unplaced.erase(std::remove(unplaced.begin(), unplaced.end(), fill), unplaced.end());
        fill->slotName = slotName;
        if (!PlaceFill(fill)) { fill->parent = this; unplaced.push_back(fill); return false; }
        return true;
    }
    return false;
}

void WidgetObjectElement::AddChild(WidgetElement* child) {
    InsertChild(child, -1);
}

void WidgetObjectElement::InsertChild(WidgetElement* child, int) {
    if (!child) return;
    if (PlaceFill(child)) return;
    child->parent = this;
    child->SetOwningWidgetRecursive(owningWidget);
    unplaced.push_back(child);
}

WidgetElement* WidgetObjectElement::DetachChild(WidgetElement* child) {
    auto it = std::find(unplaced.begin(), unplaced.end(), child);
    if (it != unplaced.end()) {
        unplaced.erase(it);
        child->parent = nullptr;
        return child;
    }
    return WidgetElement::DetachChild(child);
}

void WidgetObjectElement::RemoveChild(WidgetElement* child) {
    auto it = std::find(unplaced.begin(), unplaced.end(), child);
    if (it != unplaced.end()) {
        unplaced.erase(it);
        delete child;
        return;
    }
    WidgetElement::RemoveChild(child);
}

const std::vector<WidgetElement*>& WidgetObjectElement::GetAuthoredChildren() const {
    authoredCache.clear();
    std::vector<SlotElement*> slots;
    CollectSlots(slots);
    for (SlotElement* slot : slots) {
        for (WidgetElement* fill : slot->GetChildren()) authoredCache.push_back(fill);
    }
    for (WidgetElement* fill : unplaced) authoredCache.push_back(fill);
    return authoredCache;
}

void WidgetObjectElement::SetOwningWidgetRecursive(Widget* widget) {
    owningWidget = widget;
    if (childWidget && widget) childWidget->owningComponent = widget->owningComponent;
    std::vector<WidgetElement*> fills = GetAuthoredChildren();
    for (WidgetElement* fill : fills) fill->SetOwningWidgetRecursive(widget);
}

void WidgetObjectElement::OnTickSelf(float deltaTime, Rectangle, float) {
    if (!childWidget) return;
    if (WidgetBehavior* behavior = childWidget->GetBehavior()) behavior->OnTick(childWidget, deltaTime);
}

Rectangle WidgetObjectElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int) const {
    return child->FitToSlot(ownRect, uiScale);
}
