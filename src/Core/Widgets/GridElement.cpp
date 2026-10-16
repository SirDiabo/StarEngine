#include "GridElement.h"
#include <algorithm>

Rectangle GridElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    int cols = columns > 0 ? columns : 1;
    int col = childIndex % cols;
    int row = childIndex / cols;
    float cw = columnWidth * uiScale;
    float rh = rowHeight * uiScale;
    float spacing = cellSpacing * uiScale;
    float x = ownRect.x + col * (cw + spacing);
    float y = ownRect.y + row * (rh + spacing);
    const WidgetMargin& m = child->margin;
    float w = child->size.x > 0.0f ? child->size.x * uiScale : std::max(0.0f, cw - (m.left + m.right) * uiScale);
    float h = child->size.y > 0.0f ? child->size.y * uiScale : std::max(0.0f, rh - (m.top + m.bottom) * uiScale);
    return Rectangle{ x + m.left * uiScale, y + m.top * uiScale,
                      child->ConstrainWidth(w, uiScale), child->ConstrainHeight(h, uiScale) };
}

WidgetElement* GridElement::GetNextSelectableInDirection(WidgetElement* current, int dx, int dy) const {
    int cols = columns > 0 ? columns : 1;
    int idx = -1;
    for (int i = 0; i < (int)children.size(); i++) {
        if (children[i] == current) { idx = i; break; }
    }
    if (idx < 0) return nullptr;

    int col = idx % cols;
    int row = idx / cols;
    int newCol = col + dx;
    int newRow = row + dy;
    if (newCol < 0 || newCol >= cols) return nullptr;

    int newIndex = newRow * cols + newCol;
    if (newIndex < 0 || newIndex >= (int)children.size()) return nullptr;

    WidgetElement* candidate = children[newIndex];
    if (!candidate->selectable || candidate->disabled || !candidate->visible) return nullptr;
    return candidate;
}
