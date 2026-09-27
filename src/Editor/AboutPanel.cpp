#include "AboutPanel.h"
#include "Core/EmbeddedAssets.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <string>

AboutPanel& AboutPanel::Get() {
    static AboutPanel instance;
    return instance;
}

void AboutPanel::Open() {
    open = true;
    preventClose = true;
    if (!logoLoaded) {
        logo = EmbeddedAssets::Get().LoadTextureAsset("logo.png");
        if (logo.id != 0) logoLoaded = true;
    }
}

void AboutPanel::Close() {
    open = false;
    WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
}

void AboutPanel::Render() {
    if (!open) return;

    const UIPalette& theme = UITheme::Get();

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float boxW = 360.0f;
    float boxH = 280.0f;
    Rectangle box = { screenW * 0.5f - boxW * 0.5f, screenH * 0.5f - boxH * 0.5f, boxW, boxH };

    WidgetInput::PushModal(this, WidgetInput::kEditorDevice);

    DrawRectangle(0, 0, (int)screenW, (int)screenH, {0, 0, 0, 140});
    DrawRectangleRec(box, theme.panelBg);
    DrawRectangleLinesEx(box, 1, theme.panelBorder);

    float y = box.y + 16;
    float cx = box.x + boxW * 0.5f;

    if (logoLoaded && logo.id != 0) {
        float scale = 64.0f / (float)logo.width;
        float drawW = logo.width * scale;
        float drawH = logo.height * scale;
        DrawTexturePro(logo, {0, 0, (float)logo.width, (float)logo.height},
                       {cx - drawW * 0.5f, y, drawW, drawH}, {0, 0}, 0.0f, WHITE);
        y += drawH + 14;
    }

    const char* projectName = "StarEngine";
    int fontSize = 20;
    int textW = MeasureText(projectName, fontSize);
    DrawText(projectName, (int)(cx - textW * 0.5f), (int)y, fontSize, theme.sectionHeader);
    y += 32;

    const char* madeBy = "Made by SirDiabo";
    fontSize = 12;
    textW = MeasureText(madeBy, fontSize);
    DrawText(madeBy, (int)(cx - textW * 0.5f), (int)y, fontSize, theme.textSecondary);
    y += 24;

    Vector2 mp = GetMousePosition();

    Rectangle githubRow = { box.x + 20, y, boxW - 40, 20 };
    DrawRectangleRec(githubRow, theme.fieldBg);
    DrawRectangleLinesEx(githubRow, 1, theme.fieldBorder);
    DrawText("GitHub: SirDiabo/StarEngine", (int)(githubRow.x + 6), (int)(githubRow.y + 4), 11, {120, 160, 255, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, githubRow)) {
        OpenURL("https://github.com/SirDiabo/StarEngine");
    }
    y += 26;

    Rectangle kofiRow = { box.x + 20, y, boxW - 40, 20 };
    DrawRectangleRec(kofiRow, theme.fieldBg);
    DrawRectangleLinesEx(kofiRow, 1, theme.fieldBorder);
    DrawText("Support: ko-fi.com/sirdiabo", (int)(kofiRow.x + 6), (int)(kofiRow.y + 4), 11, {255, 160, 120, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, kofiRow)) {
        OpenURL("https://ko-fi.com/sirdiabo");
    }
    y += 30;

    Rectangle closeBtn = { box.x + boxW - 70, box.y + boxH - 32, 50, 22 };
    bool closeHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mp, closeBtn);
    DrawRectangleRec(closeBtn, closeHov ? theme.buttonHover : theme.buttonBg);
    DrawRectangleLinesEx(closeBtn, 1, theme.buttonBorder);
    int closeTextW = MeasureText("Close", 11);
    DrawText("Close", (int)(closeBtn.x + (closeBtn.width - closeTextW) / 2.0f),
             (int)(closeBtn.y + (closeBtn.height - 11) / 2.0f), 11, theme.textPrimary);
    if (closeHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Close();
    }

    if (preventClose) {
        preventClose = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, box)) {
        Close();
    }
}