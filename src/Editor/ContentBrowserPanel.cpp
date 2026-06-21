#include "ContentBrowserPanel.h"
#include "raylib.h"

void ContentBrowserPanel::Initialize() {
    name = "Content";
}

void ContentBrowserPanel::Render() {
    DrawRectangleRec(bounds, {40, 40, 40, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    if (detached) {
        DrawRectangleRec(header, {50, 50, 50, 255});
        DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
    }
}