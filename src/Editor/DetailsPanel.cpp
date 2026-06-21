#include "DetailsPanel.h"
#include "raylib.h"

void DetailsPanel::Initialize() {
    name = "Details";
}

void DetailsPanel::Render() {
    DrawText("Properties", 10, 10, 20, DARKGRAY);
}