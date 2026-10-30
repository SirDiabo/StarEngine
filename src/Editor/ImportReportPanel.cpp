#include "ImportReportPanel.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <algorithm>

ImportReportPanel& ImportReportPanel::Get() {
    static ImportReportPanel instance;
    return instance;
}

void ImportReportPanel::Open(const std::string& newTitle, const std::vector<ImportMessage>& newMessages) {
    title = newTitle;
    messages = newMessages;
    scroll = 0.0f;
    open = true;
    preventClose = true;
}

void ImportReportPanel::Close() {
    open = false;
    WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
}

void ImportReportPanel::Render() {
    if (!open) return;

    const UIPalette& theme = UITheme::Get();
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float boxW = std::min(560.0f, screenW - 40.0f);
    float boxH = std::min(380.0f, screenH - 40.0f);
    Rectangle box = { screenW * 0.5f - boxW * 0.5f, screenH * 0.5f - boxH * 0.5f, boxW, boxH };

    WidgetInput::PushModal(this, WidgetInput::kEditorDevice);

    DrawRectangle(0, 0, (int)screenW, (int)screenH, { 0, 0, 0, 140 });
    DrawRectangleRec(box, theme.panelBg);
    DrawRectangleLinesEx(box, 1, theme.panelBorder);
    DrawText(title.c_str(), (int)box.x + 12, (int)box.y + 10, 13, theme.sectionHeader);

    Rectangle list = { box.x + 10, box.y + 34, boxW - 20, boxH - 34 - 42 };
    DrawRectangleRec(list, theme.fieldBg);
    DrawRectangleLinesEx(list, 1, theme.fieldBorder);

    const float rowH = 18.0f;
    float contentH = (float)messages.size() * rowH;
    float maxScroll = std::max(0.0f, contentH - list.height + 6.0f);
    Vector2 mp = GetMousePosition();
    if (CheckCollisionPointRec(mp, list)) scroll -= GetMouseWheelMove() * rowH * 2.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    float y = list.y + 4.0f - scroll;
    for (const ImportMessage& m : messages) {
        Color color = theme.textPrimary;
        const char* tag = "info";
        if (m.level == ImportMessageLevel::Warning) { color = { 240, 200, 90, 255 }; tag = "warn"; }
        if (m.level == ImportMessageLevel::Error) { color = { 240, 110, 110, 255 }; tag = "error"; }
        DrawText(tag, (int)list.x + 8, (int)y + 3, 10, color);
        DrawText(m.text.c_str(), (int)list.x + 50, (int)y + 3, 11, color);
        y += rowH;
    }
    EndScissorMode();

    Rectangle closeBtn = { box.x + boxW - 70, box.y + boxH - 32, 56, 22 };
    bool closeHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mp, closeBtn);
    DrawRectangleRec(closeBtn, closeHov ? theme.buttonHover : theme.buttonBg);
    DrawRectangleLinesEx(closeBtn, 1, theme.buttonBorder);
    int tw = MeasureText("Close", 11);
    DrawText("Close", (int)(closeBtn.x + (closeBtn.width - tw) / 2.0f), (int)(closeBtn.y + (closeBtn.height - 11) / 2.0f), 11, theme.textPrimary);
    if (closeHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Close();

    if (preventClose) {
        preventClose = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, box)) {
        Close();
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        Close();
    }
}
