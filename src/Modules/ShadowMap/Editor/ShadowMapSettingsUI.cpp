#include "ShadowMapSettingsUI.h"
#include "Modules/ShadowMap/ShadowMapSettings.h"
#include "Editor/UITheme.h"
#include "Editor/ChoicePopup.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/CheckBoxElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <cstdio>

namespace {
    ChoicePopup resolutionPopup;
    ChoicePopup aaMethodPopup;
    Widget* fieldsChrome = nullptr;
    TextElement* sunCoverageField = nullptr;
    TextElement* shadowStrengthField = nullptr;
    CheckBoxElement* debugVisualizeBox = nullptr;
}

namespace ShadowMapSettingsUI {

ModuleSettingsResult Render(float x, float y, float width, int& activeField, bool otherPopupOpen) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    ShadowMapSettings& settings = ShadowMapSettings::Get();
    bool localAnyOpen = otherPopupOpen || resolutionPopup.IsOpen() || aaMethodPopup.IsOpen();

    if (!fieldsChrome) {
        fieldsChrome = new Widget();
        auto* root = new WidgetElement();
        fieldsChrome->SetRoot(root);

        sunCoverageField = new TextElement();
        sunCoverageField->editable = true;
        sunCoverageField->numericOnly = true;
        sunCoverageField->fontSize = 12.0f;
        sunCoverageField->backgroundColor = theme.fieldBg;
        sunCoverageField->borderColor = theme.fieldBorder;
        sunCoverageField->editingBorderColor = theme.accent;
        sunCoverageField->onTextCommitted = [](const std::string& text) {
            ShadowMapSettings::Get().SetSunCoverageHalfSize((float)atof(text.c_str()));
        };
        root->AddChild(sunCoverageField);

        shadowStrengthField = new TextElement();
        shadowStrengthField->editable = true;
        shadowStrengthField->numericOnly = true;
        shadowStrengthField->fontSize = 12.0f;
        shadowStrengthField->backgroundColor = theme.fieldBg;
        shadowStrengthField->borderColor = theme.fieldBorder;
        shadowStrengthField->editingBorderColor = theme.accent;
        shadowStrengthField->onTextCommitted = [](const std::string& text) {
            ShadowMapSettings::Get().SetShadowStrength((float)atof(text.c_str()));
        };
        root->AddChild(shadowStrengthField);

        debugVisualizeBox = new CheckBoxElement();
        debugVisualizeBox->onToggled = [](bool checked) {
            ShadowMapSettings::Get().SetDebugVisualize(checked);
        };
        root->AddChild(debugVisualizeBox);
    }

    DrawText("Shadow Map Resolution", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle resRow = {x, y, width, 20};
    DrawRectangleRec(resRow, theme.fieldBg);
    DrawRectangleLinesEx(resRow, 1, theme.fieldBorder);
    DrawText(TextFormat("%d", settings.GetShadowMapSize()), (int)resRow.x + 6, (int)resRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, resRow) && !localAnyOpen) {
        resolutionPopup.Open({resRow.x, resRow.y + resRow.height}, width, {
            {"1024", true, ""},
            {"2048", true, ""},
            {"4096", true, ""},
        });
    }
    y += 20;
    DrawText("Higher resolution looks sharper up close but costs more GPU memory and time. Point lights use half this, per cube face.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;
    int resChoice = 0;
    if (resolutionPopup.Render(resChoice)) {
        static const int sizes[3] = {1024, 2048, 4096};
        settings.SetShadowMapSize(sizes[resChoice]);
    }

    DrawText("Sun Shadow Coverage", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle coverageRow = {x, y, 120.0f, 20};
    if (!sunCoverageField->isEditing) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", settings.GetSunCoverageHalfSize());
        sunCoverageField->text = buf;
    }
    sunCoverageField->size = { coverageRow.width, coverageRow.height };
    sunCoverageField->offset = { coverageRow.x, coverageRow.y };
    y += 20;
    DrawText("Half-width, in world units, of the area around the camera the sun shadow covers. Larger covers more ground but looks blockier.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;

    DrawText("Shadow Strength", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle strengthRow = {x, y, 120.0f, 20};
    if (!shadowStrengthField->isEditing) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", settings.GetShadowStrength());
        shadowStrengthField->text = buf;
    }
    shadowStrengthField->size = { strengthRow.width, strengthRow.height };
    shadowStrengthField->offset = { strengthRow.x, strengthRow.y };
    y += 20;
    DrawText("How dark shadowed areas are, from 0 (no shadow) to 1 (fully black).",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;

    DrawText("Shadow Anti-Aliasing Method", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle aaRow = {x, y, width, 20};
    DrawRectangleRec(aaRow, theme.fieldBg);
    DrawRectangleLinesEx(aaRow, 1, theme.fieldBorder);
    DrawText(ShadowAAMethodName(settings.GetShadowAAMethod()).c_str(), (int)aaRow.x + 6, (int)aaRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, aaRow) && !localAnyOpen) {
        aaMethodPopup.Open({aaRow.x, aaRow.y + aaRow.height}, width, {
            {"None", true, ""},
            {"Low", true, ""},
            {"High", true, ""},
            {"Sharp", true, ""},
        });
    }
    y += 20;
    DrawText("Smooths jagged shadow edges. Low/High blend gradually; Sharp keeps a hard edge but positions it more accurately than None.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;
    int aaChoice = 0;
    if (aaMethodPopup.Render(aaChoice)) {
        static const ShadowAAMethod methods[4] = {ShadowAAMethod::None, ShadowAAMethod::Low, ShadowAAMethod::High, ShadowAAMethod::Sharp};
        settings.SetShadowAAMethod(methods[aaChoice]);
    }

    debugVisualizeBox->checked = settings.GetDebugVisualize();
    debugVisualizeBox->size = { 18, 18 };
    debugVisualizeBox->offset = { x, y };
    DrawText("Debug: visualize raw shadow map depth", (int)x + 24, (int)y + 3, 11, theme.textPrimary);
    y += 22;
    DrawText("Paints the raw shadow-map values. Magenta = No Active Lights. Green = Outside of shadow map's coverage area. Grayscale = Depth Value read from the shadow map.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;

    Rectangle fieldsBounds = { 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() };
    float dt = GetFrameTime();
    fieldsChrome->Tick(dt, fieldsBounds, 1.0f);
    WidgetInput::ProcessInput(fieldsChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    fieldsChrome->Draw(fieldsBounds, 1.0f);

    ModuleSettingsResult result;
    result.nextY = y;
    result.popupOpen = resolutionPopup.IsOpen() || aaMethodPopup.IsOpen();
    return result;
}

}
