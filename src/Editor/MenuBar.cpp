#include "MenuBar.h"
#include "raygui.h"

void MenuBar::Initialize() {
}

void MenuBar::Render() {
    int width = GetScreenWidth();
    Rectangle bounds = {0, 0, (float)width, 20};

    DrawRectangleRec(bounds, {50, 50, 50, 255});

    float offset = 5;
    for (size_t i = 0; i < menus.size(); i++) {
        Rectangle btnBounds = {offset, 0, 40, 20};
        if (GuiButton(btnBounds, menus[i].c_str())) {
            activeMenu = (activeMenu == (int)i) ? -1 : (int)i;
        }
        offset += 42;
    }
}