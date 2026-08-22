#include "BlobShadowsSettingsUI.h"
#include "Core/RenderSettings.h"
#include "Editor/UITheme.h"
#include "Editor/ChoicePopup.h"
#include "raylib.h"

namespace {
    ChoicePopup lightingQualityPopup;
}

namespace BlobShadowsSettingsUI {

ModuleSettingsResult Render(float x, float y, float width, int& activeField, bool otherPopupOpen) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();

    DrawText("Lighting Quality", (int)x, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle row = {x, y, width, 20};
    DrawRectangleRec(row, theme.fieldBg);
    DrawRectangleLinesEx(row, 1, theme.fieldBorder);
    LightingQuality quality = RenderSettings::Get().GetLightingQuality();
    std::string label = quality == LightingQuality::PerPixel ? "Per Pixel" : "Per Vertex";
    DrawText(label.c_str(), (int)row.x + 6, (int)row.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, row) && !otherPopupOpen) {
        lightingQualityPopup.Open({row.x, row.y + row.height}, width, {
            {"Per Vertex", true, ""},
            {"Per Pixel", true, ""},
        });
    }
    y += 20;
    DrawText("Per Vertex is cheap but can look faceted on low-poly meshes. Per Pixel costs more, looks smoother.",
              (int)x, (int)y, 10, theme.textSecondary);
    y += 24;

    int choice = 0;
    if (lightingQualityPopup.Render(choice)) {
        RenderSettings::Get().SetLightingQuality(choice == 1 ? LightingQuality::PerPixel : LightingQuality::PerVertex);
    }

    ModuleSettingsResult result;
    result.nextY = y;
    result.popupOpen = lightingQualityPopup.IsOpen();
    return result;
}

}
