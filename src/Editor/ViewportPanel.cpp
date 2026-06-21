#include "ViewportPanel.h"
#include "raylib.h"

void ViewportPanel::Initialize() {
    name = "Viewport";
}

void ViewportPanel::Render() {
    Rectangle client = {bounds.x, bounds.y + 25, bounds.width, bounds.height - 25};
    DrawRectangleRec(bounds, {30, 30, 30, 255});
    DrawRectangleRec(header, {40, 40, 40, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
}