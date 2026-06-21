#include "DetailsPanel.h"
#include "raylib.h"

void DetailsPanel::Initialize() {
    name = "Details";
}

void DetailsPanel::Render() {
    DrawRectangleRec(bounds, LIGHTGRAY);
    DrawRectangleLinesEx(bounds, 1, DARKGRAY);
    DrawText(name, (int)bounds.x + 10, (int)bounds.y + 5, 16, BLACK);
}