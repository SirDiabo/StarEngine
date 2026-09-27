#include "MenuBar.h"
#include "Core/EmbeddedAssets.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/BoxElement.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetInput.h"
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
    RebuildBar();
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

void MenuBar::RebuildBar() {
    if (!barChrome) {
        barChrome = new Widget();
        barRoot = new BoxElement();
        barRoot->axis = BoxAxis::Horizontal;
        barRoot->childSpacing = 2.0f;
        barChrome->SetRoot(barRoot);
    }
    barRoot->ClearChildren();

    const UIPalette& theme = UITheme::Get();
    for (int i = 0; i < (int)menus.size(); i++) {
        float textW = (float)MeasureText(menus[i].label.c_str(), 11);
        float btnW  = textW + MENU_BTN_PAD * 2;

        auto* btn = new ButtonElement();
        btn->size = { btnW, menuBarHeight - 2 };
        btn->defaultColor = { 0, 0, 0, 0 };
        btn->hoveredColor = theme.surfaceHover;
        btn->selectedColor = theme.surfaceHover;
        btn->pressedColor = theme.surfaceHover;
        btn->onActivated = [this, i]() {
            activeMenu = (activeMenu == i) ? -1 : i;
        };

        auto* label = new TextElement();
        label->text = menus[i].label;
        label->fontSize = 11.0f;
        label->color = theme.textPrimary;
        label->anchor = { 0.0f, 0.5f };
        label->pivot = { 0.0f, 0.5f };
        label->offset = { MENU_BTN_PAD, 0.0f };
        btn->AddChild(label);

        barRoot->AddChild(btn);
    }
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

    if (!barChrome) RebuildBar();
    for (int i = 0; i < (int)barRoot->GetChildren().size() && i < (int)menus.size(); i++) {
        ButtonElement* btn = static_cast<ButtonElement*>(barRoot->GetChildren()[i]);
        Color bg = (activeMenu == i) ? theme.surfaceHover : Color{ 0, 0, 0, 0 };
        btn->defaultColor = bg;
    }

    Rectangle barRect = { logoOffsetX, 1, (float)screenW - logoOffsetX, menuBarHeight - 2 };
    float dt = GetFrameTime();
    barChrome->Tick(dt, barRect, 1.0f);
    WidgetInput::ProcessInput(barChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    barChrome->Draw(barRect, 1.0f);
}

void MenuBar::RebuildPopup() {
    if (!popupChrome) {
        popupChrome = new Widget();
        popupRoot = new BoxElement();
        popupRoot->axis = BoxAxis::Vertical;
        popupRoot->childSpacing = 0.0f;
        popupChrome->SetRoot(popupRoot);
    }
    popupRoot->ClearChildren();
    builtPopupMenu = activeMenu;

    if (activeMenu < 0 || activeMenu >= (int)menus.size()) return;

    const UIPalette& theme = UITheme::Get();
    const MenuEntry& entry = menus[activeMenu];
    bool hasChecks = !entry.checked.empty();

    for (int index = 0; index < (int)entry.items.size(); index++) {
        const std::string& item = entry.items[index];
        if (item == "---") {
            auto* sep = new WidgetElement();
            sep->selectable = false;
            sep->size = { 0.0f, MENU_ITEM_H };
            popupRoot->AddChild(sep);
            continue;
        }

        bool checked = hasChecks && index < (int)entry.checked.size() && entry.checked[index];

        auto* row = new ButtonElement();
        row->size = { 0.0f, MENU_ITEM_H };
        row->defaultColor = { 0, 0, 0, 0 };
        row->hoveredColor = theme.surfaceHover;
        row->selectedColor = theme.surfaceHover;
        row->pressedColor = theme.surfaceHover;
        std::string menuLabel = entry.label;
        std::string itemLabel = item;
        row->onActivated = [this, menuLabel, itemLabel]() {
            if (menuLabel == "File") {
                if (itemLabel == "New Project")  wantsNewProject  = true;
                if (itemLabel == "Open Project") wantsOpenProject = true;
                if (itemLabel == "Save Project") wantsSaveProject = true;
            }
            if (onAction) onAction(menuLabel, itemLabel);
            activeMenu = -1;
        };

        if (hasChecks) {
            auto* check = new TextElement();
            check->text = checked ? "x" : "";
            check->fontSize = 11.0f;
            check->color = theme.textPrimary;
            check->anchor = { 0.0f, 0.5f };
            check->pivot = { 0.0f, 0.5f };
            check->offset = { 8.0f, 0.0f };
            row->AddChild(check);
        }

        auto* label = new TextElement();
        label->text = item;
        label->fontSize = 11.0f;
        label->color = theme.textPrimary;
        label->anchor = { 0.0f, 0.5f };
        label->pivot = { 0.0f, 0.5f };
        label->offset = { hasChecks ? (8.0f + MENU_CHECK_W) : 8.0f, 0.0f };
        row->AddChild(label);

        popupRoot->AddChild(row);
    }
}

// Draw the active popup on top of all other UI.
void MenuBar::RenderPopup() {
    if (activeMenu < 0 || activeMenu >= (int)menus.size()) {
        if (popupChrome && builtPopupMenu != -1) RebuildPopup();
        return;
    }
    if (activeMenu != builtPopupMenu) RebuildPopup();

    const UIPalette& theme = UITheme::Get();
    Vector2 mouse = GetMousePosition();
    const MenuEntry& entry = menus[activeMenu];

    float popupX = GetMenuButtonX(activeMenu);
    float popupY = menuBarHeight;
    float popupH = (float)entry.items.size() * MENU_ITEM_H + 4;
    Rectangle popup = { popupX, popupY, MENU_POPUP_W, popupH };
    Rectangle barRect = { 0, 0, (float)GetScreenWidth(), menuBarHeight };

    // Dismiss when clicking outside the popup and outside the bar buttons.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(mouse, popup) && !CheckCollisionPointRec(mouse, barRect)) {
        activeMenu = -1;
        return;
    }

    DrawRectangleRec(popup, theme.panelBg);
    DrawRectangleLinesEx(popup, 1, theme.panelBorder);

    for (int index = 0; index < (int)entry.items.size(); index++) {
        if (entry.items[index] == "---") {
            float iy = popupY + 2 + index * MENU_ITEM_H;
            DrawLine((int)(popupX + 4), (int)(iy + MENU_ITEM_H / 2),
                     (int)(popupX + MENU_POPUP_W - 4), (int)(iy + MENU_ITEM_H / 2),
                     theme.panelBorder);
        }
    }

    Rectangle body = { popupX + 1, popupY + 2, MENU_POPUP_W - 2, popupH - 4 };
    float dt = GetFrameTime();
    popupChrome->Tick(dt, body, 1.0f);
    WidgetInput::ProcessInput(popupChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    popupChrome->Draw(body, 1.0f);
}
