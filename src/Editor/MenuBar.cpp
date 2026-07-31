#include "MenuBar.h"
#include "Core/EmbeddedAssets.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"

static const float MENU_ITEM_H      = 22.0f;
static const float MENU_POPUP_W     = 160.0f;
static const float MENU_BTN_PAD     = 10.0f;
static const float MENU_CHECK_W     = 16.0f;

void MenuBar::Initialize() {
    logo = EmbeddedAssets::Get().LoadTextureAsset("logo.png");
    if (logo.id != 0) {
        logoLoaded = true;
        logoOffsetX = menuBarHeight - 4.0f + 6.0f;
    }
}

void MenuBar::SetMenus(const std::vector<MenuEntry>& entries) {
    menus    = entries;
    activeMenu = -1;
}

void MenuBar::SetActionCallback(MenuActionFn fn) {
    onAction = fn;
}

// Returns the screen-space X position of the left edge of menu button i.
float MenuBar::GetMenuButtonX(int index) const {
    float x = logoOffsetX;
    for (int i = 0; i < index; i++) {
        float textW = (float)MeasureText(menus[i].label.c_str(), 11);
        x += textW + MENU_BTN_PAD * 2 + 2;
    }
    return x;
}

// Draw the bar background and buttons. Input handling for button clicks lives here.
void MenuBar::RenderBar() {
    const UIPalette& theme = UITheme::Get();

    int screenW = GetScreenWidth();
    DrawRectangleRec({ 0, 0, (float)screenW, menuBarHeight }, theme.tabBar);

    if (logoLoaded && logo.id != 0) {
        float iconSize = menuBarHeight - 4.0f;
        DrawTexturePro(logo, {0, 0, (float)logo.width, (float)logo.height},
                       {2, 2, iconSize, iconSize}, {0, 0}, 0.0f, WHITE);
    }

    Vector2 mouse = GetMousePosition();
    float offsetX = logoOffsetX;

    for (int i = 0; i < (int)menus.size(); i++) {
        const std::string& label = menus[i].label;
        float textW = (float)MeasureText(label.c_str(), 11);
        float btnW  = textW + MENU_BTN_PAD * 2;
        Rectangle btn = { offsetX, 1, btnW, menuBarHeight - 2 };

        bool hovered = CheckCollisionPointRec(mouse, btn);
        if (hovered || activeMenu == i)
            DrawRectangleRec(btn, theme.surfaceHover);
        DrawText(label.c_str(), (int)(offsetX + MENU_BTN_PAD), 4, 11, theme.textPrimary);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            activeMenu = (activeMenu == i) ? -1 : i;

        offsetX += btnW + 2;
    }
}

// Draw the active popup on top of all other UI.
void MenuBar::RenderPopup() {
    if (activeMenu < 0 || activeMenu >= (int)menus.size()) return;

    const UIPalette& theme = UITheme::Get();
    Vector2 mouse = GetMousePosition();
    const MenuEntry& entry = menus[activeMenu];

    float popupX = GetMenuButtonX(activeMenu);
    float popupY = menuBarHeight;
    float popupH = (float)entry.items.size() * MENU_ITEM_H + 4;
    Rectangle popup = { popupX, popupY, MENU_POPUP_W, popupH };

    // Dismiss when clicking outside the popup and outside the bar buttons.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool onBarBtn = false;
        float ox = logoOffsetX;
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

    UIWidget::Panel(popup);

    float iy = popupY + 2;
    for (int index = 0; index < (int)entry.items.size(); index++) {
        const std::string& item = entry.items[index];
        if (item == "---") {
            DrawLine((int)(popupX + 4), (int)(iy + MENU_ITEM_H / 2),
                     (int)(popupX + MENU_POPUP_W - 4), (int)(iy + MENU_ITEM_H / 2),
                     theme.panelBorder);
            iy += MENU_ITEM_H;
            continue;
        }
        Rectangle row = { popupX + 1, iy, MENU_POPUP_W - 2, MENU_ITEM_H };
        bool rowHov = CheckCollisionPointRec(mouse, row);
        if (rowHov) DrawRectangleRec(row, theme.surfaceHover);

        float textX = popupX + 8;
        if (!entry.checked.empty() && index < (int)entry.checked.size() && entry.checked[index]) {
            DrawText("x", (int)(popupX + 8), (int)(iy + 5), 11, theme.textPrimary);
            textX = popupX + 8 + MENU_CHECK_W;
        }
        DrawText(item.c_str(), (int)textX, (int)(iy + 5), 11, theme.textPrimary);

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