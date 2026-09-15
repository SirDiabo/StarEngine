#include "WidgetFont.h"

namespace {
Font g_widgetFont{};
bool g_widgetFontSet = false;
}

namespace WidgetFont {

Font Get() {
    if (g_widgetFontSet) return g_widgetFont;
    return GetFontDefault();
}

void Set(Font font) {
    g_widgetFont = font;
    g_widgetFontSet = true;
}

}
