#include "ViewportPanel.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
#include "raygui.h"
#include "raymath.h"
#include <filesystem>
#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

const float ViewportPanel::toolbarHeight = 28.0f;

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

void ViewportPanel::Initialize() {
    name = "Viewport";
    camera.up = {0, 1, 0};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
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

void ViewportPanel::LoadMap(const std::string& path) {
    starMap.LoadFromFile(path);
    mapPath = path;
    mapLoaded = true;
    mapDirty = false;
    selectedActorIndex = -1;
}

void ViewportPanel::DropActorAsset(const std::string& assetPath, Vector2 screenPos) {
    if (!mapLoaded) return;

    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    Rectangle client  = {bounds.x, toolbar.y + toolbarHeight, bounds.width, bounds.height - toolbarHeight};

    if (!CheckCollisionPointRec(screenPos, client)) return;

    float u = (screenPos.x - client.x) / client.width;
    float v = (screenPos.y - client.y) / client.height;

    Ray ray = GetScreenToWorldRayEx(
        {u * (float)renderTargetWidth, v * (float)renderTargetHeight},
        camera, renderTargetWidth, renderTargetHeight);

    float worldX = 0, worldZ = 0;
    if (fabsf(ray.direction.y) > 0.001f) {
        float t = -ray.position.y / ray.direction.y;
        worldX = ray.position.x + ray.direction.x * t;
        worldZ = ray.position.z + ray.direction.z * t;
    }

    std::filesystem::path p = assetPath;
    std::string assetName = p.stem().string();

    ActorInstance inst;
    inst.name = assetName + "_" + std::to_string(starMap.GetInstances().size() + 1);
    inst.className = "Actor";
    inst.graphAssetPath = assetPath;
    inst.posX = worldX;
    inst.posY = 0;
    inst.posZ = worldZ;
    inst.scaleX = inst.scaleY = inst.scaleZ = 50.0f;

    starMap.AddInstance(inst);
    selectedActorIndex = (int)starMap.GetInstances().size() - 1;
    mapDirty = true;
}

void ViewportPanel::SaveMap() {
    if (!mapPath.empty()) {
        starMap.SaveToFile(mapPath);
        mapDirty = false;
    }
}

void ViewportPanel::UpdateCamera(Rectangle client) {
    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, client) && !flying && !orbiting) return;

    float dt = GetFrameTime();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, client) && !playing) {
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
    if (scroll != 0 && CheckCollisionPointRec(mp, client) && !playing) {
        orbitRadius -= scroll * 100.0f;
        if (orbitRadius < 100.0f) orbitRadius = 100.0f;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && CheckCollisionPointRec(mp, client) && !playing) {
        orbiting = true;
        orbitStart = mp;
    }
    if (orbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
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

    if (IsKeyPressed(KEY_F) && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        cameraTarget = {inst.posX, inst.posY, inst.posZ};
    }

    if (playing) {
        const auto& actors = World::Get().GetActors();
        if (!actors.empty()) {
            Actor* first = actors.front().get();
            cameraTarget = first->position;
        }
    }

    UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
}

static Color ClassColor(const std::string& className) {
    if (className == "Character") return {100, 180, 100, 255};
    if (className == "Pawn")      return {100, 140, 200, 255};
    return {200, 160, 80, 255};
}

void ViewportPanel::RenderScene() {
    BeginMode3D(camera);
    DrawGrid(40, 100.0f);

    if (playing) {
        for (const auto& actor : World::Get().GetActors()) {
            Vector3 pos = actor->position;
            Vector3 sc  = actor->scale;
            DrawCube(pos, sc.x, sc.y, sc.z, {100, 200, 100, 255});
            DrawCubeWires(pos, sc.x, sc.y, sc.z, {0, 0, 0, 120});
        }
    } else {
        const auto& instances = starMap.GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};
            Color color = (selectedActorIndex == (int)i) ? Color{255, 200, 50, 255} : ClassColor(inst.className);
            DrawCube(pos, inst.scaleX, inst.scaleY, inst.scaleZ, color);
            DrawCubeWires(pos, inst.scaleX, inst.scaleY, inst.scaleZ, {0, 0, 0, 120});
        }
    }

    EndMode3D();
}

void ViewportPanel::HandleMapInteraction(Rectangle client) {
    if (!mapLoaded || flying) return;
    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, client)) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selectedActorIndex = -1;

        Vector2 localMp = {mp.x - client.x, mp.y - client.y};
        Camera3D projCam = camera;
        projCam.fovy = camera.fovy;

        const auto& instances = starMap.GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};

Vector2 fboSize = {(float)renderTargetWidth, (float)renderTargetHeight};
            Matrix proj = MatrixPerspective(projCam.fovy * DEG2RAD,
                                             fboSize.x / fboSize.y, 1.0f, 50000.0f);
            Matrix view = MatrixLookAt(projCam.position, projCam.target, projCam.up);
            Matrix mvp  = MatrixMultiply(view, proj);

            Vector4 clip = {
                pos.x * mvp.m0 + pos.y * mvp.m4 + pos.z * mvp.m8  + mvp.m12,
                pos.x * mvp.m1 + pos.y * mvp.m5 + pos.z * mvp.m9  + mvp.m13,
                pos.x * mvp.m2 + pos.y * mvp.m6 + pos.z * mvp.m10 + mvp.m14,
                pos.x * mvp.m3 + pos.y * mvp.m7 + pos.z * mvp.m11 + mvp.m15
            };

            if (clip.w <= 0) continue;

            Vector2 ndc = {clip.x / clip.w, clip.y / clip.w};
            Vector2 screen = {
                (ndc.x + 1.0f) * 0.5f * fboSize.x,
                (1.0f - ndc.y) * 0.5f * fboSize.y
            };

            if (CheckCollisionPointCircle(localMp, screen, 20.0f)) {
                selectedActorIndex = (int)i;
                break;
            }
        }
    }

    if (IsKeyPressed(KEY_DELETE) && selectedActorIndex >= 0) {
        starMap.RemoveInstance((size_t)selectedActorIndex);
        selectedActorIndex = -1;
        mapDirty = true;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && mapDirty) SaveMap();
}

void ViewportPanel::RenderToolbar() {
    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, {36, 36, 40, 255});
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), {60, 60, 65, 255});

    DrawText(name, (int)bounds.x + 6, (int)bounds.y + 8, 13, WHITE);

    float btnRight = bounds.x + bounds.width - 4;
    float btnY = toolbar.y + 3;

    if (!playing) {
Rectangle playBtn = {btnRight - 22, btnY, 22, 22};
         DrawRectangleRec(playBtn, {30, 130, 60, 255});
         DrawText(">", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);
         if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), playBtn)) {
             playing = true;
             savedYaw = yaw;
             savedPitch = pitch;
             savedOrbitRadius = orbitRadius;
             savedCameraTarget = cameraTarget;
             if (flying) { flying = false; ShowCursor(); }
             for (const ActorInstance& inst : starMap.GetInstances()) {
                 Actor* actor = World::Get().SpawnActor(inst.className, inst.name);
                 actor->position = {inst.posX, inst.posY, inst.posZ};
                 actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
                 actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};
                 if (!inst.graphAssetPath.empty() && fs::exists(inst.graphAssetPath)) {
                     auto* behavior = new StargraphBehavior(Stargraph{});
                     behavior->GetGraph().LoadFromFile(inst.graphAssetPath);
                     actor->SetGraphBehavior(behavior);
                 }
             }
             World::Get().BeginPlay();
         }
         btnRight -= 26;
    } else {
Rectangle stopBtn = {btnRight - 22, btnY, 22, 22};
         DrawRectangleRec(stopBtn, {160, 40, 40, 255});
         DrawText("S", (int)stopBtn.x + 7, (int)stopBtn.y + 4, 13, WHITE);
if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), stopBtn)) {
              World::Get().EndPlay();
              yaw = savedYaw;
              pitch = savedPitch;
              orbitRadius = savedOrbitRadius;
              cameraTarget = savedCameraTarget;
              UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
              if (flying) { flying = false; ShowCursor(); }
              playing = false;
          }
          btnRight -= 26;
    }

    if (mapLoaded) {
        Rectangle saveBtn = {btnRight - 50, btnY, 50, 22};
        if (GuiButton(saveBtn, "Save")) SaveMap();
        btnRight -= 54;
        if (mapDirty) DrawText("*", (int)btnRight - 10, (int)btnY + 4, 14, {200, 160, 60, 255});
    }
}

void ViewportPanel::Render() {
    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    Rectangle client  = {bounds.x, toolbar.y + toolbarHeight, bounds.width, bounds.height - toolbarHeight};
    int clientWidth   = (int)client.width;
    int clientHeight  = (int)client.height;

    if (clientWidth > 0 && clientHeight > 0) {
        EnsureRenderTarget(clientWidth, clientHeight);
        UpdateCamera(client);
        HandleMapInteraction(client);

        BeginTextureMode(renderTarget);
        ClearBackground({28, 28, 30, 255});
        RenderScene();
        EndTextureMode();

        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        DrawTextureRec(renderTarget.texture, source, {client.x, client.y}, WHITE);
    }

    DrawRectangleLinesEx(bounds, 1, {70, 70, 75, 255});

    if (playing) {
        DrawRectangleLinesEx(bounds, 3, {60, 180, 60, 255});
        const char* pieLabel = "PLAYING - Press S to Stop";
        int tw = MeasureText(pieLabel, 12);
        DrawText(pieLabel, (int)(bounds.x + bounds.width / 2 - tw / 2),
                 (int)(bounds.y + toolbarHeight + 4), 12, {60, 200, 60, 255});
    }
    RenderToolbar();

    if (mapLoaded && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        char info[128];
        snprintf(info, sizeof(info), "%s (%s)", inst.name.c_str(), inst.className.c_str());
        DrawText(info, (int)bounds.x + 8, (int)(client.y + 6), 12, {200, 200, 210, 255});
    }

    if (flying) {
        const char* hint = "WASD fly  Q/E up/down  Scroll=speed  Release RMB to exit";
        DrawText(hint, (int)bounds.x + 8, (int)(client.y + client.height - 20), 11, {160, 160, 170, 200});
    }

    if (!mapLoaded) {
        const char* msg = "Open a .starmap file from the Content Browser";
        int tw = MeasureText(msg, 13);
        DrawText(msg, (int)(bounds.x + bounds.width / 2 - tw / 2),
                 (int)(bounds.y + bounds.height / 2), 13, {100, 100, 110, 255});
    }
}