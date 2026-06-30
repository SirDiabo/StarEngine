#include "LogPanel.h"
#include "raylib.h"

void LogPanel::Initialize() {
    name = "Logs";
}

void LogPanel::AddLog(const std::string& message) {
    logs.push_back(message);
    while ((int)logs.size() > maxLogs)
        logs.pop_front();
}

void LogPanel::Render() {
    DrawRectangleRec(bounds, { 40, 40, 40, 255 });
    DrawRectangleLinesEx(bounds, 1, { 80, 80, 80, 255 });

    float y    = bounds.y + 4;
    float maxY = bounds.y + bounds.height - 10;
    for (auto it = logs.rbegin(); it != logs.rend() && y < maxY; ++it) {
        const std::string& msg = *it;
        Color color = { 200, 200, 200, 255 };
        if (msg.rfind("WARNING", 0) == 0 || msg.rfind("WARN:", 0) == 0)
            color = { 240, 200, 60, 255 };
        else if (msg.rfind("ERROR", 0) == 0 || msg.rfind("FATAL", 0) == 0)
            color = { 230, 80, 80, 255 };
        DrawText(msg.c_str(), (int)bounds.x + 5, (int)y, 10, color);
        y += 15;
    }
}