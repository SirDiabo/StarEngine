#include "EditorSettingsTab.h"
#include "MenuBar.h"
#include "PropertyFields.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "NativeFileDialog.h"
#include "raylib.h"
#include <string>
#include <filesystem>
#include <cstdlib>

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

static std::string StarEngineDir() {
    return (std::filesystem::path(getenv("USERPROFILE") ? getenv("USERPROFILE") :
            (getenv("HOME") ? getenv("HOME") : ".")) / ".starengine").string();
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
    float maxRowWidth = area.width - 32 > 460.0f ? 460.0f : area.width - 32;

    DrawText("Theme", (int)fx, (int)fy, 14, theme.accent);
    fy += 22;
    DrawText("Editor color scheme.", (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 22;

    int presetCount = UITheme::PresetCount();
    int activePreset = UITheme::GetPresetIndex();
    float btnX = fx;
    float btnY = fy;
    const float btnH = 24.0f;
    const float btnPad = 14.0f;
    const float btnGapX = 6.0f;
    const float btnGapY = 6.0f;

    Rectangle customBtn = {btnX, btnY, (float)MeasureText("Custom", 11) + btnPad * 2, btnH};
    if (ThemeOptionButton(customBtn, "Custom", activePreset == -1)) {
        std::string path = NativeFileDialog::OpenFile(
            "Import Theme", StarEngineDir(), "StarEngine Theme", "*.startheme");
        if (!path.empty()) UITheme::ImportFromFile(path);
    }
    btnX += customBtn.width + btnGapX;

    for (int i = 0; i < presetCount; i++) {
        const char* name = UITheme::PresetName(i);
        float btnW = (float)MeasureText(name, 11) + btnPad * 2;
        if (btnX + btnW > fx + maxRowWidth && btnX > fx) {
            btnX = fx;
            btnY += btnH + btnGapY;
        }
        Rectangle btn = {btnX, btnY, btnW, btnH};
        if (ThemeOptionButton(btn, name, i == activePreset)) {
            UITheme::SetPreset(i);
        }
        btnX += btnW + btnGapX;
    }
    fy = btnY + btnH + 14.0f;

    int fieldCount = UITheme::FieldCount();

    DrawText("Colors", (int)fx, (int)fy, 14, theme.accent);
    Rectangle resetBtn = {fx + maxRowWidth - 90, fy - 3, 90, 20};
    if (UIWidget::Button(resetBtn, "Reset Colors")) {
        UITheme::ClearAllOverrides();
    }
    fy += 22;
    DrawText("Individual overrides for the active preset. Click a swatch to edit.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 20;

    Rectangle listArea = {area.x, fy, area.width, area.y + area.height - fy - 42};
    float rowH = 22.0f;
    float rowWidth = area.width - 32 > 360.0f ? 360.0f : area.width - 32;
    float contentHeight = fieldCount * rowH;

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, listArea)) {
        colorListScroll -= GetMouseWheelMove() * 20.0f;
    }
    float maxScroll = contentHeight > listArea.height ? contentHeight - listArea.height : 0.0f;
    if (colorListScroll < 0.0f) colorListScroll = 0.0f;
    if (colorListScroll > maxScroll) colorListScroll = maxScroll;

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    float y = listArea.y - colorListScroll;
    for (int i = 0; i < fieldCount; i++) {
        if (y + rowH >= listArea.y && y <= listArea.y + listArea.height) {
            const UIColorFieldInfo& field = UITheme::Field(i);
            Rectangle row = {fx, y, rowWidth, rowH - 2};

            Color value;
            if (colorPicker.IsOpen() && editingColorField == i) {
                value = colorPicker.GetLiveColor();
            } else {
                value = theme.*(field.member);
            }

            std::string label = field.name;
            if (UITheme::HasOverride(i)) label += " *";

            if (PropertyField::ColorField(row, label.c_str(), value) && !colorPicker.IsOpen()) {
                colorPicker.Open({row.x + row.width, row.y}, value);
                editingColorField = i;
            }
        }
        y += rowH;
    }
    EndScissorMode();

    Rectangle exportBtn = {fx, listArea.y + listArea.height + 8, rowWidth, 24};
    if (UIWidget::Button(exportBtn, "Export Theme...")) {
        std::string path = NativeFileDialog::SaveFile(
            "Export Theme", StarEngineDir(), "my_theme.startheme", "StarEngine Theme", "*.startheme");
        if (!path.empty()) {
            if (path.size() < 10 || path.substr(path.size() - 10) != ".startheme")
                path += ".startheme";
            UITheme::ExportToFile(path);
        }
    }

    if (colorPicker.IsOpen()) {
        Color result;
        bool confirmed = false;
        if (colorPicker.Render(result, confirmed)) {
            if (confirmed && editingColorField >= 0) {
                UITheme::SetOverride(editingColorField, result);
            }
            editingColorField = -1;
        }
    } else {
        editingColorField = -1;
    }
}

void EditorSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}