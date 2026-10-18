#include "SlotElement.h"
#include "raylib.h"

bool SlotElement::showPlaceholders = false;

void SlotElement::OnDrawSelf(Rectangle rect, float uiScale) {
    if (!showPlaceholders || !children.empty()) return;
    DrawRectangleLinesEx(rect, 1.0f, Color{ 120, 170, 255, 160 });
    DrawText(slotName.c_str(), (int)rect.x + 4, (int)rect.y + 3, (int)(10.0f * uiScale), Color{ 120, 170, 255, 200 });
}

Rectangle SlotElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    return child->FitToSlot(ownRect, uiScale);
}
