#include "LogPanel.h"
#include "raylib.h"

void LogPanel::Initialize() {
    name = "Logs";
}

void LogPanel::Render() {
    DrawRectangleRec(bounds, LIGHTGRAY);
    DrawRectangleLinesEx(bounds, 1, DARKGRAY);
    DrawText(name, (int)bounds.x + 10, (int)bounds.y + 5, 16, BLACK);

    float y = bounds.y + 30;
    float maxY = bounds.y + bounds.height - 20;
    for (auto it = logs.rbegin(); it != logs.rend() && y < maxY; ++it) {
        DrawText(it->c_str(), (int)bounds.x + 10, (int)y, 10, BLACK);
        y += 15;
    }
}

void LogPanel::AddLog(const std::string& message) {
    if ((int)logs.size() >= maxLogs) logs.pop_front();
    logs.push_back(message);
}