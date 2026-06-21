#include "ContentBrowserPanel.h"
#include "raylib.h"

void ContentBrowserPanel::Initialize() {
    name = "Content";
}

void ContentBrowserPanel::Render() {
    DrawText("Content Browser", 10, 10, 20, DARKGRAY);
    DrawText("Right-click to create new assets", 10, 40, 12, GRAY);
}