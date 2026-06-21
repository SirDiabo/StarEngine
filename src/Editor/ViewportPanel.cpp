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

ViewportPanel::~ViewportPanel() {
    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
}

void ViewportPanel::EnsureRenderTarget(int width, int height) {
    if (width == renderTargetWidth && height == renderTargetHeight) return;

    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    renderTarget = LoadRenderTexture(width, height);
    renderTargetWidth = width;
    renderTargetHeight = height;
}

void ViewportPanel::Render() {
    Rectangle client = { bounds.x, bounds.y + 25, bounds.width, bounds.height - 25 };
    int clientWidth = (int)client.width;
    int clientHeight = (int)client.height;

    if (clientWidth > 0 && clientHeight > 0) {
        EnsureRenderTarget(clientWidth, clientHeight);

        BeginTextureMode(renderTarget);
        ClearBackground({ 30, 30, 30, 255 });
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        EndMode3D();
        EndTextureMode();

        Rectangle source = { 0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
        DrawTextureRec(renderTarget.texture, source, Vector2{ client.x, client.y }, WHITE);
    }

    DrawRectangleRec(header, {40, 40, 40, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
}