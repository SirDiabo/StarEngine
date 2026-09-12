#include "TextElement.h"

void TextElement::OnDrawSelf(Rectangle rect, float uiScale) {
    float scaledSize = fontSize * uiScale;
    float spacing = scaledSize / 10.0f;
    Vector2 textSize = MeasureTextEx(GetFontDefault(), text.c_str(), scaledSize, spacing);
    float drawX = rect.x + (rect.width - textSize.x) * pivot.x;
    float drawY = rect.y + (rect.height - textSize.y) * pivot.y;
    DrawTextEx(GetFontDefault(), text.c_str(), {drawX, drawY}, scaledSize, spacing, color);
}
