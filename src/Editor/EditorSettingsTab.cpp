#include "EditorSettingsTab.h"
#include "MenuBar.h"
#include "PropertyFields.h"
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

struct ColorFieldEntry {
    const char* label;
    Color UIPalette::* member;
};

static const ColorFieldEntry kColorFields[] = {
    {"Window Background",       &UIPalette::windowBg},
    {"Panel Background",        &UIPalette::panelBg},
    {"Panel Border",            &UIPalette::panelBorder},
    {"Tab Bar",                 &UIPalette::tabBar},
    {"Tab Text",                &UIPalette::tabText},
    {"Hover Surface",           &UIPalette::surfaceHover},
    {"Selected Surface",        &UIPalette::surfaceSelected},
    {"Splitter",                &UIPalette::splitter},
    {"Splitter (Hover)",        &UIPalette::splitterHover},
    {"Drop Zone",               &UIPalette::dropZone},
    {"Drop Zone Border",        &UIPalette::dropBorder},
    {"Button",                  &UIPalette::buttonBg},
    {"Button (Hover)",          &UIPalette::buttonHover},
    {"Button (Pressed)",        &UIPalette::buttonPressed},
    {"Button Border",           &UIPalette::buttonBorder},
    {"Button Text",             &UIPalette::buttonText},
    {"Button Text (Disabled)",  &UIPalette::buttonDisabledText},
    {"Field Background",        &UIPalette::fieldBg},
    {"Field Border",            &UIPalette::fieldBorder},
    {"Primary Text",            &UIPalette::textPrimary},
    {"Secondary Text",          &UIPalette::textSecondary},
    {"Section Header",          &UIPalette::sectionHeader},
    {"Accent",                  &UIPalette::accent},
    {"Danger (Hover)",          &UIPalette::danger},
    {"Danger",                  &UIPalette::dangerBg},
    {"Success (Hover)",         &UIPalette::success},
    {"Success",                 &UIPalette::successBg},
};
static const int kColorFieldCount = sizeof(kColorFields) / sizeof(kColorFields[0]);

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
    if (ThemeOptionButton(darkBtn, "Dark", isDark))    UITheme::SetMode(ThemeMode::Dark);
    if (ThemeOptionButton(lightBtn, "Light", !isDark)) UITheme::SetMode(ThemeMode::Light);
    fy += 36;

    DrawText("Colors", (int)fx, (int)fy, 14, theme.accent);
    fy += 22;
    DrawText("Individual overrides for the active preset.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 20;

    Rectangle listArea = {area.x, fy, area.width, area.y + area.height - fy - 8};
    float rowH = 22.0f;
    float rowWidth = area.width - 32 > 360.0f ? 360.0f : area.width - 32;
    float contentHeight = kColorFieldCount * rowH;

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, listArea)) {
        colorListScroll -= GetMouseWheelMove() * 20.0f;
    }
    float maxScroll = contentHeight > listArea.height ? contentHeight - listArea.height : 0.0f;
    if (colorListScroll < 0.0f) colorListScroll = 0.0f;
    if (colorListScroll > maxScroll) colorListScroll = maxScroll;

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    float y = listArea.y - colorListScroll;
    for (int i = 0; i < kColorFieldCount; i++) {
        if (y + rowH >= listArea.y && y <= listArea.y + listArea.height) {
            Rectangle row = {fx, y, rowWidth, rowH - 2};
            Color value = theme.*(kColorFields[i].member);
            PropertyField::ColorField(row, kColorFields[i].label, value);
        }
        y += rowH;
    }
    EndScissorMode();
}

void EditorSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}