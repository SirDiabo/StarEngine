#include "ProgressBarElement.h"

void ProgressBarElement::OnDrawSelf(Rectangle rect, float uiScale) {
    DrawRectangleRec(rect, backgroundColor);
    float clamped = value;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > 1.0f) clamped = 1.0f;
    Rectangle fillRect{ rect.x, rect.y, rect.width * clamped, rect.height };
    DrawRectangleRec(fillRect, fillColor);
}
