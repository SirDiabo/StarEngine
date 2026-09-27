#include "LogPanel.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"

void LogPanel::Initialize() {
    name = "Logs";
}

void LogPanel::AddLog(const std::string& message, LogLevel level) {
    logs.push_back({ message, level });
    totalPushed++;
    while ((int)logs.size() > maxLogs)
        logs.pop_front();
}

std::vector<LogEntry> LogPanel::GetLogsSince(int fromCount) const {
    std::vector<LogEntry> result;
    int firstIndex = totalPushed - (int)logs.size();
    int offset = fromCount - firstIndex;
    if (offset < 0) offset = 0;
    for (int i = offset; i < (int)logs.size(); i++)
        result.push_back(logs[i]);
    return result;
}

static Color LogEntryColor(LogLevel level) {
    const UIPalette& theme = UITheme::Get();
    switch (level) {
        case LogLevel::Warning: return { 240, 200, 60, 255 };
        case LogLevel::Error:   return theme.danger;
        default:                return theme.textPrimary;
    }
}

void LogPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    float lineHeight = 15.0f;
    float padding = 4.0f;
    float contentHeight = logs.size() * lineHeight;
    float visibleHeight = bounds.height - padding * 2;

    Vector2 mouse = GetMousePosition();
    Rectangle contentArea = { bounds.x + padding, bounds.y + padding, bounds.width - padding * 2, visibleHeight };
    if (!WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mouse, contentArea)) {
        scrollOffset -= GetMouseWheelMove() * lineHeight * 3;
    }

    float maxScroll = (contentHeight > visibleHeight) ? (contentHeight - visibleHeight) : 0.0f;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    BeginScissorMode((int)bounds.x + 1, (int)bounds.y + 1, (int)bounds.width - 2, (int)bounds.height - 2);
    float y = bounds.y + padding - scrollOffset;
    for (auto it = logs.rbegin(); it != logs.rend() && y < bounds.y + bounds.height - padding; ++it) {
        DrawText(it->message.c_str(), (int)bounds.x + 5, (int)y, 10, LogEntryColor(it->level));
        y += lineHeight;
    }
    EndScissorMode();

    if (contentHeight > visibleHeight) {
        float scrollRatio = (maxScroll > 0) ? scrollOffset / maxScroll : 0;
        float barHeight = (visibleHeight / contentHeight) * visibleHeight;
        float barY = bounds.y + padding + scrollRatio * (visibleHeight - barHeight);
        DrawRectangleRec({ bounds.x + bounds.width - 7, barY, 4, barHeight }, { 100, 100, 100, 180 });
        DrawRectangleLinesEx({ bounds.x + bounds.width - 7, barY, 4, barHeight }, 1, { 150, 150, 150, 255 });
    }
}