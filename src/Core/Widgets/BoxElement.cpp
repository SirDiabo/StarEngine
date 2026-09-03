#include "BoxElement.h"

Rectangle BoxElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    float running = -scrollOffset * uiScale;
    for (int i = 0; i < childIndex; i++) {
        WidgetElement* sibling = children[i];
        if (!sibling->visible) continue;
        float extent = (axis == BoxAxis::Vertical ? sibling->size.y : sibling->size.x) * uiScale;
        running += extent + childSpacing * uiScale;
    }

    float childW = child->size.x * uiScale;
    float childH = child->size.y * uiScale;

    if (axis == BoxAxis::Vertical) {
        return Rectangle{ ownRect.x, ownRect.y + running, childW > 0.0f ? childW : ownRect.width, childH };
    }

    return Rectangle{ ownRect.x + running, ownRect.y, childW, childH > 0.0f ? childH : ownRect.height };
}

float BoxElement::GetContentExtent(float uiScale) const {
    float total = 0.0f;
    for (int i = 0; i < (int)children.size(); i++) {
        if (!children[i]->visible) continue;
        float extent = (axis == BoxAxis::Vertical ? children[i]->size.y : children[i]->size.x) * uiScale;
        total += extent + childSpacing * uiScale;
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
