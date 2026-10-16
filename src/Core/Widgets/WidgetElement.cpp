#include "WidgetElement.h"
#include "Widget.h"
#include <algorithm>

namespace {
int nextWidgetElementId = 1;

bool ContainsRecursive(WidgetElement* root, WidgetElement* target) {
    if (!root || !target) return false;
    if (root == target) return true;
    for (WidgetElement* child : root->GetChildren()) {
        if (ContainsRecursive(child, target)) return true;
    }
    return false;
}
}

WidgetElement::WidgetElement() {
    elementId = std::to_string(nextWidgetElementId++);
}

void WidgetElement::EnsureNextElementIdAtLeast(int id) {
    if (id >= nextWidgetElementId) nextWidgetElementId = id + 1;
}

WidgetElement::~WidgetElement() {
    for (WidgetElement* child : children) delete child;
}

void WidgetElement::AddChild(WidgetElement* child) {
    if (!child) return;
    child->parent = this;
    child->SetOwningWidgetRecursive(owningWidget);
    children.push_back(child);
}

void WidgetElement::InsertChild(WidgetElement* child, int index) {
    if (!child) return;
    child->parent = this;
    child->SetOwningWidgetRecursive(owningWidget);
    if (index < 0 || index > (int)children.size()) index = (int)children.size();
    children.insert(children.begin() + index, child);
}

static void RememberSelection(Widget* widget) {
    if (widget && widget->currentSelection) widget->pendingSelectionKey = widget->currentSelection->selectionKey;
}

WidgetElement* WidgetElement::DetachChild(WidgetElement* child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it == children.end()) return nullptr;
    if (owningWidget && ContainsRecursive(*it, owningWidget->currentSelection)) {
        RememberSelection(owningWidget);
        owningWidget->currentSelection = nullptr;
    }
    WidgetElement* detached = *it;
    children.erase(it);
    detached->parent = nullptr;
    return detached;
}

void WidgetElement::SetOwningWidgetRecursive(Widget* widget) {
    owningWidget = widget;
    for (WidgetElement* child : children) child->SetOwningWidgetRecursive(widget);
}

void WidgetElement::RemoveChild(WidgetElement* child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it == children.end()) return;
    if (owningWidget && ContainsRecursive(*it, owningWidget->currentSelection)) {
        RememberSelection(owningWidget);
        owningWidget->currentSelection = nullptr;
    }
    delete *it;
    children.erase(it);
}

void WidgetElement::RemoveChildAt(int index) {
    if (index < 0 || index >= (int)children.size()) return;
    if (owningWidget && ContainsRecursive(children[index], owningWidget->currentSelection)) {
        RememberSelection(owningWidget);
        owningWidget->currentSelection = nullptr;
    }
    delete children[index];
    children.erase(children.begin() + index);
}

void WidgetElement::ClearChildren() {
    if (owningWidget && owningWidget->currentSelection) {
        for (WidgetElement* child : children) {
            if (ContainsRecursive(child, owningWidget->currentSelection)) {
                RememberSelection(owningWidget);
        owningWidget->currentSelection = nullptr;
                break;
            }
        }
    }
    for (WidgetElement* child : children) delete child;
    children.clear();
}

Rectangle WidgetElement::InsetByMargin(Rectangle slot, float uiScale) const {
    if (margin.IsZero()) return slot;
    float left = margin.left * uiScale;
    float top = margin.top * uiScale;
    float width = slot.width - left - margin.right * uiScale;
    float height = slot.height - top - margin.bottom * uiScale;
    return Rectangle{ slot.x + left, slot.y + top, std::max(0.0f, width), std::max(0.0f, height) };
}

float WidgetElement::ConstrainWidth(float width, float uiScale) const {
    if (maxSize.x > 0.0f) width = std::min(width, maxSize.x * uiScale);
    if (minSize.x > 0.0f) width = std::max(width, minSize.x * uiScale);
    return width;
}

float WidgetElement::ConstrainHeight(float height, float uiScale) const {
    if (maxSize.y > 0.0f) height = std::min(height, maxSize.y * uiScale);
    if (minSize.y > 0.0f) height = std::max(height, minSize.y * uiScale);
    return height;
}

Rectangle WidgetElement::FitToSlot(Rectangle slot, float uiScale) const {
    Rectangle inner = InsetByMargin(slot, uiScale);
    inner.width = ConstrainWidth(inner.width, uiScale);
    inner.height = ConstrainHeight(inner.height, uiScale);
    return inner;
}

Rectangle WidgetElement::ResolveRect(Rectangle parentRect, float uiScale) const {
    parentRect = InsetByMargin(parentRect, uiScale);
    Vector2 anchorPoint{ parentRect.x + parentRect.width * anchor.x, parentRect.y + parentRect.height * anchor.y };
    Vector2 scaledSize{ ConstrainWidth(size.x * uiScale, uiScale), ConstrainHeight(size.y * uiScale, uiScale) };
    Vector2 topLeft{
        anchorPoint.x + offset.x * uiScale - scaledSize.x * pivot.x,
        anchorPoint.y + offset.y * uiScale - scaledSize.y * pivot.y
    };
    return Rectangle{ topLeft.x, topLeft.y, scaledSize.x, scaledSize.y };
}

Rectangle WidgetElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    return child->ResolveRect(ownRect, uiScale);
}

void WidgetElement::DrawChildren(Rectangle rect, float uiScale) {
    for (int i = 0; i < (int)children.size(); i++) {
        WidgetElement* child = children[i];
        if (!child->visible) continue;
        Rectangle childRect = ComputeChildRect(child, rect, uiScale, i);
        child->Draw(childRect, uiScale);
    }
}

void WidgetElement::TickChildren(float deltaTime, Rectangle rect, float uiScale) {
    for (int i = 0; i < (int)children.size(); i++) {
        WidgetElement* child = children[i];
        if (!child->visible) continue;
        Rectangle childRect = ComputeChildRect(child, rect, uiScale, i);
        child->Tick(deltaTime, childRect, uiScale);
    }
}

void WidgetElement::Draw(Rectangle rect, float uiScale) {
    OnDrawSelf(rect, uiScale);
    DrawChildren(rect, uiScale);
}

void WidgetElement::Tick(float deltaTime, Rectangle rect, float uiScale) {
    lastResolvedRect = rect;
    OnTickSelf(deltaTime, rect, uiScale);
    TickChildren(deltaTime, rect, uiScale);
}

void WidgetElement::CollectSelectables(std::vector<WidgetElement*>& out) {
    if (selectable && !disabled) out.push_back(this);
    for (WidgetElement* child : children) {
        if (!child->visible) continue;
        child->CollectSelectables(out);
    }
}

void WidgetElement::OnSelected() {
    if (owningWidget) owningWidget->FireElementEvent(this, "Selected");
}

void WidgetElement::OnDeselected() {
    if (owningWidget) owningWidget->FireElementEvent(this, "Deselected");
}

void WidgetElement::OnConfirmed() {
    if (owningWidget && !RaisesPointerEvents()) owningWidget->FireElementEvent(this, "Clicked");
    if (onActivated) onActivated();
}
