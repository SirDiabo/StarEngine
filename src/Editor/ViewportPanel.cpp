#include "ViewportPanel.h"
#include "raylib.h"

void ViewportPanel::Initialize() {
    name = "Viewport";
    camera.position = Vector3{ 10, 10, 10 };
    camera.target = Vector3{ 0, 0, 0 };
    camera.up = Vector3{ 0, 1, 0 };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void ViewportPanel::Render() {
    BeginMode3D(camera);
    ClearBackground(RAYWHITE);
    DrawGrid(10, 1.0f);
    EndMode3D();
}