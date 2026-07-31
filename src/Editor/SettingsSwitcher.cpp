#include "SettingsSwitcher.h"
#include "UITheme.h"
#include "UIWidgets.h"

static const float SIDEBAR_WIDTH = 160.0f;
static const float ROW_HEIGHT    = 28.0f;

void SettingsSwitcher::SetCategories(const std::vector<std::string>& names) {
    categories = names;
    if (selected >= (int)categories.size()) selected = 0;
}

Rectangle SettingsSwitcher::Render(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(area);

    Rectangle sidebar = {area.x, area.y, SIDEBAR_WIDTH, area.height};
    Rectangle content  = {area.x + SIDEBAR_WIDTH, area.y, area.width - SIDEBAR_WIDTH, area.height};

    DrawRectangleRec(sidebar, theme.tabBar);

    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < (int)categories.size(); i++) {
        Rectangle row = {sidebar.x, sidebar.y + i * ROW_HEIGHT, sidebar.width, ROW_HEIGHT};
        bool hovered = CheckCollisionPointRec(mouse, row);
        bool active  = (i == selected);

        DrawRectangleRec(row, UIWidget::SelectableBg(hovered, active, theme.tabBar));
        if (active) DrawRectangle((int)row.x, (int)row.y, 3, (int)row.height, theme.accent);
        DrawText(categories[i].c_str(), (int)row.x + 14, (int)(row.y + (row.height - 11) / 2.0f), 11,
                 active ? theme.textPrimary : theme.textSecondary);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selected = i;
    }

    return content;
}
