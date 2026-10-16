#include "BoxElement.h"
#include <algorithm>

float BoxElement::GetSlotExtent(const WidgetElement* element, float uiScale) const {
    if (axis == BoxAxis::Vertical) {
        return (element->margin.top + element->margin.bottom) * uiScale + element->ConstrainHeight(element->size.y * uiScale, uiScale);
    }
    return (element->margin.left + element->margin.right) * uiScale + element->ConstrainWidth(element->size.x * uiScale, uiScale);
}

Rectangle BoxElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    float running = -scrollOffset * uiScale;
    for (int i = 0; i < childIndex; i++) {
        WidgetElement* sibling = children[i];
        if (!sibling->visible) continue;
        running += GetSlotExtent(sibling, uiScale) + childSpacing * uiScale;
    }

    const WidgetMargin& m = child->margin;
    float childW = child->size.x * uiScale;
    float childH = child->size.y * uiScale;

    if (axis == BoxAxis::Vertical) {
        float width = childW > 0.0f ? childW : std::max(0.0f, ownRect.width - (m.left + m.right) * uiScale);
        return Rectangle{ ownRect.x + m.left * uiScale, ownRect.y + running + m.top * uiScale,
                          child->ConstrainWidth(width, uiScale), child->ConstrainHeight(childH, uiScale) };
    }

    float height = childH > 0.0f ? childH : std::max(0.0f, ownRect.height - (m.top + m.bottom) * uiScale);
    return Rectangle{ ownRect.x + running + m.left * uiScale, ownRect.y + m.top * uiScale,
                      child->ConstrainWidth(childW, uiScale), child->ConstrainHeight(height, uiScale) };
}

float BoxElement::GetContentExtent(float uiScale) const {
    float total = 0.0f;
    for (int i = 0; i < (int)children.size(); i++) {
        if (!children[i]->visible) continue;
        total += GetSlotExtent(children[i], uiScale) + childSpacing * uiScale;
    }
    return total;
}

void BoxElement::Draw(Rectangle rect, float uiScale) {
    if (backgroundColor.a > 0) DrawRectangleRec(rect, backgroundColor);

    if (scrollEnabled) {
        BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
        DrawChildren(rect, uiScale);
        EndScissorMode();
        return;
    }

    DrawChildren(rect, uiScale);
}

WidgetElement* BoxElement::GetNextSelectableInDirection(WidgetElement* current, int dx, int dy) const {
    int step = (axis == BoxAxis::Vertical) ? dy : dx;
    if (step == 0) return nullptr;

    int idx = -1;
    for (int i = 0; i < (int)children.size(); i++) {
        if (children[i] == current) { idx = i; break; }
    }
    if (idx < 0) return nullptr;

    int newIndex = idx + (step > 0 ? 1 : -1);
    while (newIndex >= 0 && newIndex < (int)children.size()) {
        WidgetElement* candidate = children[newIndex];
        if (candidate->selectable && !candidate->disabled && candidate->visible) return candidate;
        newIndex += (step > 0 ? 1 : -1);
    }

    if (wrapNavigation) {
        int wrapStart = (step > 0) ? 0 : (int)children.size() - 1;
        int wrapIndex = wrapStart;
        while (wrapIndex >= 0 && wrapIndex < (int)children.size() && wrapIndex != idx) {
            WidgetElement* candidate = children[wrapIndex];
            if (candidate->selectable && !candidate->disabled && candidate->visible) return candidate;
            wrapIndex += (step > 0 ? 1 : -1);
        }
    }

    return nullptr;
}
