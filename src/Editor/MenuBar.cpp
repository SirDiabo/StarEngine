#include "MenuBar.h"
#include "raygui.h"

void MenuBar::Initialize() {
}

void MenuBar::Render() {
    int width = GetScreenWidth();
    Rectangle bounds = {0, 0, (float)width, 30};

    DrawRectangleRec(bounds, {50, 50, 50, 255});

    float offset = 5;
    for (size_t i = 0; i < menus.size(); i++) {
        Rectangle btnBounds = {offset, 0, 60, 30};
        if (GuiButton(btnBounds, menus[i].c_str())) {
            activeMenu = (int)i;
        }
        offset += 65;
    }

    if (activeMenu == 0) {
        Rectangle popup = {5, 30, 120, 100};
        Rectangle newBtn = {10, 35, 110, 25};
        Rectangle openBtn = {10, 65, 110, 25};
        Rectangle saveBtn = {10, 95, 110, 25};

        DrawRectangleRec(popup, {60, 60, 60, 255});
        if (GuiButton(newBtn, "New Project")) wantsNewProject = true;
        if (GuiButton(openBtn, "Open Project")) wantsOpenProject = true;
        if (GuiButton(saveBtn, "Save Project")) wantsSaveProject = true;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), popup)) {
            activeMenu = -1;
        }
    }
}