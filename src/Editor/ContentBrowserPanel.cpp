#include "ContentBrowserPanel.h"
#include "raylib.h"

void ContentBrowserPanel::Initialize() {
    name = "Content";
}

void ContentBrowserPanel::Render() {
    DrawRectangleRec(bounds, LIGHTGRAY);
    DrawRectangleLinesEx(bounds, 1, DARKGRAY);
    DrawText(name, (int)bounds.x + 10, (int)bounds.y + 5, 16, BLACK);
}