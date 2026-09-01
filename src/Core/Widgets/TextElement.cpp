#include "TextElement.h"

void TextElement::OnDrawSelf(Rectangle rect, float uiScale) {
    DrawText(text.c_str(), (int)rect.x, (int)rect.y, (int)(fontSize * uiScale), color);
}
