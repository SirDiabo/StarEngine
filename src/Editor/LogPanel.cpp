#include "LogPanel.h"
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
    switch (level) {
        case LogLevel::Warning: return { 240, 200, 60, 255 };
        case LogLevel::Error:   return { 230, 80, 80, 255 };
        default:                return { 200, 200, 200, 255 };
    }
}

void LogPanel::Render() {
    DrawRectangleRec(bounds, { 40, 40, 40, 255 });
    DrawRectangleLinesEx(bounds, 1, { 80, 80, 80, 255 });

    float y    = bounds.y + 4;
    float maxY = bounds.y + bounds.height - 10;
    for (auto it = logs.rbegin(); it != logs.rend() && y < maxY; ++it) {
        DrawText(it->message.c_str(), (int)bounds.x + 5, (int)y, 10, LogEntryColor(it->level));
        y += 15;
    }
}