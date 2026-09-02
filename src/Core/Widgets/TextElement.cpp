#include "TextElement.h"

void TextElement::OnDrawSelf(Rectangle rect, float uiScale) {
    float scaledSize = fontSize * uiScale;
    float spacing = scaledSize / 10.0f;
    DrawTextEx(GetFontDefault(), text.c_str(), {rect.x, rect.y}, scaledSize, spacing, color);
}
