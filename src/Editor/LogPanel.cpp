#include "LogPanel.h"
#include "raylib.h"

void LogPanel::Initialize() {
    name = "Logs";
}

void LogPanel::Render() {
    DrawRectangleRec(bounds, {40, 40, 40, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    if (detached) {
        DrawRectangleRec(header, {50, 50, 50, 255});
        DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
    }

    float y = bounds.y + (detached ? 25 : 0);
    float maxY = bounds.y + bounds.height - 10;
    for (auto it = logs.rbegin(); it != logs.rend() && y < maxY; ++it) {
        DrawText(it->c_str(), (int)bounds.x + 5, (int)y, 10, {200, 200, 200, 255});
        y += 15;
    }
}