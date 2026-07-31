#include "EditorSettingsTab.h"
#include "MenuBar.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raylib.h"

static bool ThemeOptionButton(Rectangle r, const char* label, bool active) {
    UIWidget::Interaction s = UIWidget::Interact(r);
    const UIPalette& theme = UITheme::Get();

    DrawRectangleRec(r, UIWidget::SelectableBg(s.hovered, active, theme.buttonBg));
    DrawRectangleLinesEx(r, 1, active ? theme.accent : theme.buttonBorder);

    int tw = MeasureText(label, 11);
    DrawText(label, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - 11) / 2.0f), 11,
             active ? theme.textPrimary : theme.textSecondary);

    return s.clicked;
}

void EditorSettingsTab::Initialize() {
    title     = "Editor Settings";
    permanent = false;
    switcher.SetCategories({ "Style" });
}

void EditorSettingsTab::Render(float x, float y, float width, float height) {
    Rectangle content = switcher.Render({x, y, width, height});

    switch (switcher.Selected()) {
        case 0: RenderStyleCategory(content); break;
        default: break;
    }
}

void EditorSettingsTab::RenderStyleCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();

    float fx = area.x + 16;
    float fy = area.y + 16;

    DrawText("Theme", (int)fx, (int)fy, 14, theme.accent);
    fy += 22;
    DrawText("Editor color scheme.", (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 22;

    Rectangle darkBtn  = {fx, fy, 90, 24};
    Rectangle lightBtn = {fx + 96, fy, 90, 24};

    bool isDark = UITheme::GetMode() == ThemeMode::Dark;
    if (ThemeOptionButton(darkBtn, "Dark", isDark))   UITheme::SetMode(ThemeMode::Dark);
    if (ThemeOptionButton(lightBtn, "Light", !isDark)) UITheme::SetMode(ThemeMode::Light);
}

void EditorSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
