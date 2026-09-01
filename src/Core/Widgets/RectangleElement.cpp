#include "RectangleElement.h"

void RectangleElement::OnDrawSelf(Rectangle rect, float uiScale) {
    DrawRectangleRec(rect, color);
}
