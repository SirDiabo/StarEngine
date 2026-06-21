#include "DetailsPanel.h"
#include "raylib.h"

void DetailsPanel::Initialize() {
    name = "Details";
}

void DetailsPanel::Render() {
    DrawRectangleRec(bounds, {40, 40, 40, 255});
    DrawRectangleRec(header, {50, 50, 50, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
}