#include "Core/RotationMath.h"
#include "AssetPreviewViewport.h"
#include "Runtime/AssetCache.h"
#include "Core/StarMaterial.h"
#include "UITheme.h"
#include "raymath.h"
#include "rlgl.h"
#include <filesystem>
#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

const float AssetPreviewViewport::toolbarHeight = 28.0f;

static void UpdateOrbitPosition(Camera3D& camera, float yaw, float pitch, float radius, Vector3 target) {
    float yr = yaw   * DEG2RAD;
    float pr = pitch * DEG2RAD;
    camera.position = {
        target.x + cosf(pr) * sinf(yr) * radius,
        target.y + sinf(pr) * radius,
        target.z + cosf(pr) * cosf(yr) * radius
    };
    camera.target = target;
}

void AssetPreviewViewport::Initialize() {
    camera.up = {0, 1, 0};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
}

AssetPreviewViewport::~AssetPreviewViewport() {
    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
}

void AssetPreviewViewport::SetAsset(const std::string& starAssetPath) {
    assetPath = starAssetPath;
    StarAsset::LoadFromFile(assetPath, asset);
}

void AssetPreviewViewport::OnReimport() {
    if (!asset.sourcePath.empty()) AssetCache::Get().ReloadModel(asset.ResolveSourcePath());
    StarAsset::LoadFromFile(assetPath, asset);
}

void AssetPreviewViewport::EnsureRenderTarget(int width, int height) {
    if (width == renderTargetWidth && height == renderTargetHeight) return;
    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    renderTarget = LoadRenderTexture(width, height);
    renderTargetWidth = width;
    renderTargetHeight = height;
}

void AssetPreviewViewport::UpdateCamera(Rectangle client) {
    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, client) && !flying && !orbiting) return;

    float dt = GetFrameTime();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, client)) {
        flying = true;
        lookStart = mp;
        flyPosition = camera.position;
        yaw   = yaw + 180.0f;
        pitch = -pitch;
        HideCursor();
    }

    if (flying) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = {mp.x - lookStart.x, mp.y - lookStart.y};
            yaw   -= delta.x * 0.3f;
            pitch -= delta.y * 0.3f;
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
            SetMousePosition((int)lookStart.x, (int)lookStart.y);

            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                flySpeed += wheel * 100.0f;
                if (flySpeed < 40.0f)   flySpeed = 40.0f;
                if (flySpeed > 4000.0f) flySpeed = 4000.0f;
            }

            float yr = yaw   * DEG2RAD;
            float pr = pitch * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            Vector3 right   = {cosf(yr), 0.0f, -sinf(yr)};
            Vector3 up      = {0.0f, 1.0f, 0.0f};

            if (IsKeyDown(KEY_W)) flyPosition = Vector3Add(flyPosition, Vector3Scale(forward,   flySpeed * dt));
            if (IsKeyDown(KEY_S)) flyPosition = Vector3Add(flyPosition, Vector3Scale(forward,  -flySpeed * dt));
            if (IsKeyDown(KEY_A)) flyPosition = Vector3Add(flyPosition, Vector3Scale(right,     flySpeed * dt));
            if (IsKeyDown(KEY_D)) flyPosition = Vector3Add(flyPosition, Vector3Scale(right,    -flySpeed * dt));
            if (IsKeyDown(KEY_E)) flyPosition = Vector3Add(flyPosition, Vector3Scale(up,        flySpeed * dt));
            if (IsKeyDown(KEY_Q)) flyPosition = Vector3Add(flyPosition, Vector3Scale(up,       -flySpeed * dt));

            camera.position = flyPosition;
            camera.target   = Vector3Add(flyPosition, forward);
        } else {
            flying = false;
            ShowCursor();
            yaw   = yaw + 180.0f;
            pitch = -pitch;
            float yr = yaw   * DEG2RAD;
            float pr = pitch * DEG2RAD;
            Vector3 lookDir = {cosf(-pr) * sinf(yr + 3.14159265f), sinf(-pr), cosf(-pr) * cosf(yr + 3.14159265f)};
            cameraTarget = Vector3Add(flyPosition, Vector3Scale(lookDir, orbitRadius));
        }
        return;
    }

    float scroll = GetMouseWheelMove();
    if (scroll != 0 && CheckCollisionPointRec(mp, client)) {
        orbitRadius -= scroll * 100.0f;
        if (orbitRadius < 50.0f) orbitRadius = 50.0f;
    }

    bool altOrbitHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if ((IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
        && CheckCollisionPointRec(mp, client)) {
        orbiting = true;
        orbitStart = mp;
    }
    if (orbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
            Vector2 delta = {mp.x - orbitStart.x, mp.y - orbitStart.y};
            yaw   -= delta.x * 0.4f;
            pitch += delta.y * 0.4f;
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
            orbitStart = mp;
        } else {
            orbiting = false;
        }
    }

    UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
}

void AssetPreviewViewport::RenderScene() {
    BeginMode3D(camera);

    DrawGrid(20, 100.0f);

    // Reload the cooked asset each frame so inline import-detail edits (materials,
    // transform) show live without a manual reimport.
    if (!assetPath.empty()) StarAsset::LoadFromFile(assetPath, asset);

    std::vector<std::string> matPaths;
    for (const MaterialSlot& slot : asset.materialSlots) matPaths.push_back(slot.materialPath);

    Vector3 pos = {asset.importPosX, asset.importPosY, asset.importPosZ};
    Vector3 rot = {asset.importRotX, asset.importRotY, asset.importRotZ};
    Vector3 scl = {asset.importScaleX, asset.importScaleY, asset.importScaleZ};

    Model* model = nullptr;
    if (!asset.sourcePath.empty()) {
        model = AssetCache::Get().GetOrLoadModel(asset.ResolveSourcePath());
    }

    if (model) {
        Matrix scaleMatrix = MatrixScale(scl.x, scl.y, scl.z);
        Matrix rotationMatrix = ComposeRotationDegrees(rot);
        Matrix savedTransform = model->transform;
        model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);

        AssetCache::ApplyMaterialSlots(model, matPaths);
        if (previewAnim != nullptr) UpdateModelAnimation(*model, *previewAnim, previewAnimFrame);
        DrawModel(*model, pos, 1.0f, WHITE);
        DrawModelWires(*model, pos, 1.0f, {255, 200, 50, 120});
        model->transform = savedTransform;
    } else {
        Color fallback = {200, 160, 80, 255};
        DrawCube(pos, scl.x, scl.y, scl.z, fallback);
        DrawCubeWires(pos, scl.x, scl.y, scl.z, {0, 0, 0, 120});
        if (!asset.sourcePath.empty() && !fs::exists(asset.ResolveSourcePath())) {
            DrawCubeWires(pos, scl.x * 1.08f, scl.y * 1.08f, scl.z * 1.08f, {220, 40, 200, 200});
        }
    }

    EndMode3D();
}

void AssetPreviewViewport::Render(float x, float y, float width, float height) {
    Rectangle client = {x, y, width, height};
    int clientWidth  = (int)client.width;
    int clientHeight = (int)client.height;

    if (clientWidth > 0 && clientHeight > 0) {
        EnsureRenderTarget(clientWidth, clientHeight);
        UpdateCamera(client);

        BeginTextureMode(renderTarget);
        ClearBackground({28, 28, 30, 255});
        RenderScene();
        EndTextureMode();

        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        DrawTextureRec(renderTarget.texture, source, {client.x, client.y}, WHITE);
    }

    const UIPalette& theme = UITheme::Get();
    DrawRectangleLinesEx(client, 1, theme.panelBorder);

    if (flying) {
        const char* hint = "WASD fly  Q/E up/down  Scroll=speed  Release RMB to exit";
        DrawText(hint, (int)(client.x + 8), (int)(client.y + client.height - 20), 11, {160, 160, 170, 200});
    }

    if (!asset.sourcePath.empty() && !fs::exists(asset.ResolveSourcePath())) {
        const char* msg = "Source file missing. Reimport to fix.";
        int tw = MeasureText(msg, 12);
        DrawText(msg, (int)(client.x + client.width / 2 - tw / 2),
                 (int)(client.y + client.height / 2), 12, theme.danger);
    }
}
