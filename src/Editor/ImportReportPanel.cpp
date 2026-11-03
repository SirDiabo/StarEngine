#include "ImportReportPanel.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <algorithm>
#include <vector>

namespace {

std::vector<std::string> WrapText(const std::string& text, float maxWidth, int fontSize) {
    std::vector<std::string> lines;
    std::string current;
    size_t i = 0;
    while (i < text.size()) {
        size_t len = 1;
        unsigned char lead = (unsigned char)text[i];
        if (lead >= 0xF0) len = 4; else if (lead >= 0xE0) len = 3; else if (lead >= 0xC0) len = 2;
        len = std::min(len, text.size() - i);
        std::string ch = text.substr(i, len);
        if (!current.empty() && (float)MeasureText((current + ch).c_str(), fontSize) > maxWidth) {
            size_t space = current.find_last_of(' ');
            if (ch != " " && space != std::string::npos && space > 0) {
                lines.push_back(current.substr(0, space));
                current = current.substr(space + 1);
            } else {
                lines.push_back(current);
                current.clear();
            }
        }
        if (!(current.empty() && ch == " " && !lines.empty())) current += ch;
        i += len;
    }
    if (!current.empty() || lines.empty()) lines.push_back(current);
    return lines;
}

}

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
    const float lineH = 14.0f;
    const float textX = list.x + 50.0f;
    const float textW = list.x + list.width - textX - 8.0f;
    std::vector<std::vector<std::string>> wrapped;
    wrapped.reserve(messages.size());
    float contentH = 0.0f;
    for (const ImportMessage& m : messages) {
        wrapped.push_back(WrapText(m.text, textW, 11));
        contentH += std::max(rowH, 4.0f + (float)wrapped.back().size() * lineH);
    }
    float maxScroll = std::max(0.0f, contentH - list.height + 6.0f);
    Vector2 mp = GetMousePosition();
    if (CheckCollisionPointRec(mp, list)) scroll -= GetMouseWheelMove() * rowH * 2.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    float y = list.y + 4.0f - scroll;
    for (size_t mi = 0; mi < messages.size(); mi++) {
        const ImportMessage& m = messages[mi];
        Color color = theme.textPrimary;
        const char* tag = "info";
        if (m.level == ImportMessageLevel::Warning) { color = { 240, 200, 90, 255 }; tag = "warn"; }
        if (m.level == ImportMessageLevel::Error) { color = { 240, 110, 110, 255 }; tag = "error"; }
        DrawText(tag, (int)list.x + 8, (int)y + 3, 10, color);
        const std::vector<std::string>& rows = wrapped[mi];
        for (size_t li = 0; li < rows.size(); li++)
            DrawText(rows[li].c_str(), (int)textX, (int)(y + 3.0f + (float)li * lineH), 11, color);
        y += std::max(rowH, 4.0f + (float)rows.size() * lineH);
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
