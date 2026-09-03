#include "WidgetElement.h"
#include "Widget.h"
#include <algorithm>

WidgetElement::~WidgetElement() {
    for (WidgetElement* child : children) delete child;
}

void WidgetElement::AddChild(WidgetElement* child) {
    if (!child) return;
    child->parent = this;
    child->SetOwningWidgetRecursive(owningWidget);
    children.push_back(child);
}

void WidgetElement::SetOwningWidgetRecursive(Widget* widget) {
    owningWidget = widget;
    for (WidgetElement* child : children) child->SetOwningWidgetRecursive(widget);
}

void WidgetElement::RemoveChild(WidgetElement* child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it == children.end()) return;
    delete *it;
    children.erase(it);
}

void WidgetElement::RemoveChildAt(int index) {
    if (index < 0 || index >= (int)children.size()) return;
    delete children[index];
    children.erase(children.begin() + index);
}

void WidgetElement::ClearChildren() {
    for (WidgetElement* child : children) delete child;
    children.clear();
}

Rectangle WidgetElement::ResolveRect(Rectangle parentRect, float uiScale) const {
    Vector2 anchorPoint{ parentRect.x + parentRect.width * anchor.x, parentRect.y + parentRect.height * anchor.y };
    Vector2 scaledSize{ size.x * uiScale, size.y * uiScale };
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
    if (owningWidget) owningWidget->FireElementEvent(this, "Clicked");
}
