#include "RectangleElement.h"

void RectangleElement::OnDrawSelf(Rectangle rect, float uiScale) {
    if (cornerRadius > 0.0f) {
        DrawRectangleRounded(rect, cornerRadius, 4, color);
        if (borderThickness > 0.0f && borderColor.a > 0) {
            DrawRectangleRoundedLinesEx(rect, cornerRadius, 4, borderThickness, borderColor);
        }
        return;
    }
    DrawRectangleRec(rect, color);
    if (borderThickness > 0.0f && borderColor.a > 0) {
        DrawRectangleLinesEx(rect, borderThickness, borderColor);
    }
}
