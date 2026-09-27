#include "LoadingPanel.h"
#include "UITheme.h"
#include <cmath>

LoadingPanel& LoadingPanel::Get() {
    static LoadingPanel instance;
    return instance;
}

void LoadingPanel::Show(const std::string& titleText) {
    visible = true;
    title = titleText;
    statusLine.clear();
}

void LoadingPanel::Hide() {
    visible = false;
}

void LoadingPanel::SetStatusLine(const std::string& line) {
    statusLine = line;
}

void LoadingPanel::Render() {
    if (!visible) return;

    const UIPalette& theme = UITheme::Get();
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();

    DrawRectangleRec({0, 0, screenW, screenH}, {0, 0, 0, 160});

    float boxW = 520.0f;
    float boxH = 110.0f;
    Rectangle box = {screenW * 0.5f - boxW * 0.5f, screenH * 0.5f - boxH * 0.5f, boxW, boxH};
    DrawRectangleRec(box, theme.panelBg);
    DrawRectangleLinesEx(box, 1, theme.panelBorder);

    DrawText(title.c_str(), (int)box.x + 16, (int)box.y + 16, 16, theme.textPrimary);

    std::string line = statusLine;
    size_t maxChars = 78;
    if (line.size() > maxChars) {
        line = "..." + line.substr(line.size() - (maxChars - 3));
    }
    DrawText(line.c_str(), (int)box.x + 16, (int)box.y + 48, 11, theme.textSecondary);

    Rectangle barBg = {box.x + 16, box.y + 74, boxW - 32, 14};
    DrawRectangleRec(barBg, theme.fieldBg);
    DrawRectangleLinesEx(barBg, 1, theme.fieldBorder);

    float segmentWidth = barBg.width * 0.28f;
    float travel = barBg.width - segmentWidth;
    float phase = fmodf((float)GetTime() * 220.0f, travel * 2.0f);
    float segX = travel - fabsf(phase - travel);
    Rectangle segment = {barBg.x + segX, barBg.y, segmentWidth, barBg.height};
    DrawRectangleRec(segment, theme.accent);
}
