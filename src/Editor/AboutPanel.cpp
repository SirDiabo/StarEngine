#include "AboutPanel.h"
#include "Core/EmbeddedAssets.h"
#include "raylib.h"
#include "raygui.h"
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
}

void AboutPanel::Render() {
    if (!open) return;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float boxW = 360.0f;
    float boxH = 280.0f;
    Rectangle box = { screenW * 0.5f - boxW * 0.5f, screenH * 0.5f - boxH * 0.5f, boxW, boxH };

    DrawRectangle(0, 0, (int)screenW, (int)screenH, {0, 0, 0, 140});
    DrawRectangleRec(box, {40, 40, 46, 255});
    DrawRectangleLinesEx(box, 1, {80, 80, 90, 255});

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
    DrawText(projectName, (int)(cx - textW * 0.5f), (int)y, fontSize, {200, 210, 230, 255});
    y += 32;

    const char* madeBy = "Made by SirDiabo";
    fontSize = 12;
    textW = MeasureText(madeBy, fontSize);
    DrawText(madeBy, (int)(cx - textW * 0.5f), (int)y, fontSize, {160, 170, 190, 255});
    y += 24;

    Vector2 mp = GetMousePosition();

    Rectangle githubRow = { box.x + 20, y, boxW - 40, 20 };
    DrawRectangleRec(githubRow, {30, 30, 32, 255});
    DrawRectangleLinesEx(githubRow, 1, {55, 55, 60, 255});
    DrawText("GitHub: SirDiabo/StarEngine", (int)(githubRow.x + 6), (int)(githubRow.y + 4), 11, {120, 160, 255, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, githubRow)) {
        OpenURL("https://github.com/SirDiabo/StarEngine");
    }
    y += 26;

    Rectangle kofiRow = { box.x + 20, y, boxW - 40, 20 };
    DrawRectangleRec(kofiRow, {30, 30, 32, 255});
    DrawRectangleLinesEx(kofiRow, 1, {55, 55, 60, 255});
    DrawText("Support: ko-fi.com/sirdiabo", (int)(kofiRow.x + 6), (int)(kofiRow.y + 4), 11, {255, 160, 120, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, kofiRow)) {
        OpenURL("https://ko-fi.com/sirdiabo");
    }
    y += 30;

    Rectangle closeBtn = { box.x + boxW - 70, box.y + boxH - 32, 50, 22 };
    if (GuiButton(closeBtn, "Close")) {
        Close();
    }

    if (preventClose) {
        preventClose = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, box)) {
        Close();
    }
}
