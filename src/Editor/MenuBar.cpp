#include "MenuBar.h"
#include "raygui.h"
#include "raylib.h"

static const Color COLOR_MENUBAR_BG = { 45, 45, 48, 255 };
static const Color COLOR_MENU_POPUP = { 55, 55, 60, 255 };
static const Color COLOR_MENU_HOVER = { 70, 70, 80, 255 };
static const Color COLOR_MENU_SEP   = { 75, 75, 80, 255 };
static const Color COLOR_MENU_TEXT  = { 210, 210, 210, 255 };
static const float MENU_ITEM_H      = 22.0f;
static const float MENU_POPUP_W     = 160.0f;
static const float MENU_BTN_PAD     = 10.0f;
static const float MENU_CHECK_W     = 16.0f;

void MenuBar::Initialize() {}

void MenuBar::SetMenus(const std::vector<MenuEntry>& entries) {
    menus    = entries;
    activeMenu = -1;
}

void MenuBar::SetActionCallback(MenuActionFn fn) {
    onAction = fn;
}

// Returns the screen-space X position of the left edge of menu button i.
float MenuBar::GetMenuButtonX(int index) const {
    float x = 4.0f;
    for (int i = 0; i < index; i++) {
        float textW = (float)MeasureText(menus[i].label.c_str(), 11);
        x += textW + MENU_BTN_PAD * 2 + 2;
    }
    return x;
}

// Draw the bar background and buttons. Input handling for button clicks lives here.
void MenuBar::RenderBar() {
    int screenW = GetScreenWidth();
    DrawRectangleRec({ 0, 0, (float)screenW, menuBarHeight }, COLOR_MENUBAR_BG);

    Vector2 mouse = GetMousePosition();
    float offsetX = 4.0f;

    for (int i = 0; i < (int)menus.size(); i++) {
        const std::string& label = menus[i].label;
        float textW = (float)MeasureText(label.c_str(), 11);
        float btnW  = textW + MENU_BTN_PAD * 2;
        Rectangle btn = { offsetX, 1, btnW, menuBarHeight - 2 };

        bool hovered = CheckCollisionPointRec(mouse, btn);
        if (hovered || activeMenu == i)
            DrawRectangleRec(btn, COLOR_MENU_HOVER);
        DrawText(label.c_str(), (int)(offsetX + MENU_BTN_PAD), 4, 11, COLOR_MENU_TEXT);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            activeMenu = (activeMenu == i) ? -1 : i;

        offsetX += btnW + 2;
    }
}

// Draw the active popup on top of all other UI.
void MenuBar::RenderPopup() {
    if (activeMenu < 0 || activeMenu >= (int)menus.size()) return;

    Vector2 mouse = GetMousePosition();
    const MenuEntry& entry = menus[activeMenu];

    float popupX = GetMenuButtonX(activeMenu);
    float popupY = menuBarHeight;
    float popupH = (float)entry.items.size() * MENU_ITEM_H + 4;
    Rectangle popup = { popupX, popupY, MENU_POPUP_W, popupH };

    // Dismiss when clicking outside the popup and outside the bar buttons.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool onBarBtn = false;
        float ox = 4.0f;
        for (int i = 0; i < (int)menus.size(); i++) {
            float textW = (float)MeasureText(menus[i].label.c_str(), 11);
            float btnW  = textW + MENU_BTN_PAD * 2;
            if (CheckCollisionPointRec(mouse, { ox, 1, btnW, menuBarHeight - 2 })) {
                onBarBtn = true;
                break;
            }
            ox += btnW + 2;
        }
        if (!onBarBtn && !CheckCollisionPointRec(mouse, popup))
            activeMenu = -1;
        if (activeMenu < 0) return;
    }

    DrawRectangleRec(popup, COLOR_MENU_POPUP);
    DrawRectangleLinesEx(popup, 1, { 80, 80, 88, 255 });

    float iy = popupY + 2;
    for (int index = 0; index < (int)entry.items.size(); index++) {
        const std::string& item = entry.items[index];
        if (item == "---") {
            DrawLine((int)(popupX + 4), (int)(iy + MENU_ITEM_H / 2),
                     (int)(popupX + MENU_POPUP_W - 4), (int)(iy + MENU_ITEM_H / 2),
                     COLOR_MENU_SEP);
            iy += MENU_ITEM_H;
            continue;
        }
        Rectangle row = { popupX + 1, iy, MENU_POPUP_W - 2, MENU_ITEM_H };
        bool rowHov = CheckCollisionPointRec(mouse, row);
        if (rowHov) DrawRectangleRec(row, COLOR_MENU_HOVER);

        float textX = popupX + 8;
        if (!entry.checked.empty() && index < (int)entry.checked.size() && entry.checked[index]) {
            DrawText("x", (int)(popupX + 8), (int)(iy + 5), 11, COLOR_MENU_TEXT);
            textX = popupX + 8 + MENU_CHECK_W;
        }
        DrawText(item.c_str(), (int)textX, (int)(iy + 5), 11, COLOR_MENU_TEXT);

        if (rowHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (entry.label == "File") {
                if (item == "New Project")  wantsNewProject  = true;
                if (item == "Open Project") wantsOpenProject = true;
                if (item == "Save Project") wantsSaveProject = true;
            }
            if (onAction) onAction(entry.label, item);
            activeMenu = -1;
        }
        iy += MENU_ITEM_H;
    }
}