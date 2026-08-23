#include "ShadowMapSettingsUI.h"
#include "Modules/ShadowMap/ShadowMapSettings.h"
#include "Editor/UITheme.h"
#include "Editor/ChoicePopup.h"
#include "Editor/PropertyFields.h"
#include "raylib.h"
#include <cstdio>

namespace {
    ChoicePopup resolutionPopup;
    ChoicePopup aaMethodPopup;
    char sunCoverageBuf[16] = "";
    char shadowStrengthBuf[16] = "";
    bool buffersInitialized = false;
}

namespace ShadowMapSettingsUI {

ModuleSettingsResult Render(float x, float y, float width, int& activeField, bool otherPopupOpen) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    ShadowMapSettings& settings = ShadowMapSettings::Get();
    bool localAnyOpen = otherPopupOpen || resolutionPopup.IsOpen() || aaMethodPopup.IsOpen();

    if (!buffersInitialized) {
        snprintf(sunCoverageBuf, sizeof(sunCoverageBuf), "%.0f", settings.GetSunCoverageHalfSize());
        snprintf(shadowStrengthBuf, sizeof(shadowStrengthBuf), "%.2f", settings.GetShadowStrength());
        buffersInitialized = true;
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
    float coverageValue = settings.GetSunCoverageHalfSize();
    float prevCoverage = coverageValue;
    PropertyField::FloatBox(coverageRow, sunCoverageBuf, sizeof(sunCoverageBuf), 2001, activeField, coverageValue);
    if (coverageValue != prevCoverage) settings.SetSunCoverageHalfSize(coverageValue);
    y += 20;
    DrawText("Half-width, in world units, of the area around the camera the sun shadow covers. Larger covers more ground but looks blockier.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;

    DrawText("Shadow Strength", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle strengthRow = {x, y, 120.0f, 20};
    float strengthValue = settings.GetShadowStrength();
    float prevStrength = strengthValue;
    PropertyField::FloatBox(strengthRow, shadowStrengthBuf, sizeof(shadowStrengthBuf), 2002, activeField, strengthValue);
    if (strengthValue != prevStrength) settings.SetShadowStrength(strengthValue);
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

    ModuleSettingsResult result;
    result.nextY = y;
    result.popupOpen = resolutionPopup.IsOpen() || aaMethodPopup.IsOpen();
    return result;
}

}
