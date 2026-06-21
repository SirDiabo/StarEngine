#include "ViewportPanel.h"
#include "raylib.h"

void ViewportPanel::Initialize() {
    name = "Viewport";
}

void ViewportPanel::Render() {
    DrawRectangleRec(bounds, RAYWHITE);
    DrawRectangleLinesEx(bounds, 1, DARKGRAY);
    DrawText(name, (int)bounds.x + 10, (int)bounds.y + 5, 16, BLACK);
}