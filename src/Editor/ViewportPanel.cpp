#include "ViewportPanel.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "raygui.h"
#include "raymath.h"
#include <filesystem>
#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

const float ViewportPanel::toolbarHeight = 28.0f;

void ViewportPanel::Initialize() {
    name = "Viewport";
    camera.up = Vector3{0, 1, 0};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float yawRad   = yaw   * DEG2RAD;
    float pitchRad = pitch * DEG2RAD;
    camera.position = {
        cosf(pitchRad) * sinf(yawRad) * orbitRadius,
        sinf(pitchRad) * orbitRadius,
        cosf(pitchRad) * cosf(yawRad) * orbitRadius
    };
    camera.target = {0, 0, 0};
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

void ViewportPanel::SaveMap() {
    if (!mapPath.empty()) {
        starMap.SaveToFile(mapPath);
        mapDirty = false;
    }
}

void ViewportPanel::UpdateCamera(Rectangle graphArea) {
    Vector2 mousePos = GetMousePosition();
    if (!CheckCollisionPointRec(mousePos, graphArea)) return;

    float scroll = GetMouseWheelMove();
    if (scroll != 0) {
        orbitRadius -= scroll * 1.5f;
        if (orbitRadius < 2.0f) orbitRadius = 2.0f;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        orbiting = true;
        orbitStart = mousePos;
    }
    if (orbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 delta = {mousePos.x - orbitStart.x, mousePos.y - orbitStart.y};
            yaw   -= delta.x * 0.4f;
            pitch += delta.y * 0.4f;
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
            orbitStart = mousePos;
        } else {
            orbiting = false;
        }
    }

    float yawRad   = yaw   * DEG2RAD;
    float pitchRad = pitch * DEG2RAD;
    camera.position = {
        cosf(pitchRad) * sinf(yawRad) * orbitRadius,
        sinf(pitchRad) * orbitRadius,
        cosf(pitchRad) * cosf(yawRad) * orbitRadius
    };
}

static Color ClassColor(const std::string& className) {
    if (className == "Character") return {100, 180, 100, 255};
    if (className == "Pawn")      return {100, 140, 200, 255};
    return {200, 160, 80, 255};
}

void ViewportPanel::RenderScene() {
    BeginMode3D(camera);

    DrawGrid(40, 1.0f);

    const auto& instances = starMap.GetInstances();
    for (size_t i = 0; i < instances.size(); i++) {
        const ActorInstance& inst = instances[i];
        Vector3 pos = {inst.posX, inst.posY, inst.posZ};
        Color color = (selectedActorIndex == (int)i) ? Color{255, 200, 50, 255} : ClassColor(inst.className);
        DrawCube(pos, inst.scaleX, inst.scaleY, inst.scaleZ, color);
        DrawCubeWires(pos, inst.scaleX, inst.scaleY, inst.scaleZ, {0, 0, 0, 120});
        DrawBillboard(camera, {0}, pos + Vector3{0, inst.scaleY * 0.5f + 0.3f, 0}, 0.3f, WHITE);
    }

    EndMode3D();
}

void ViewportPanel::HandleMapInteraction(Rectangle graphArea) {
    if (!mapLoaded) return;
    Vector2 mousePos = GetMousePosition();
    if (!CheckCollisionPointRec(mousePos, graphArea)) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        actorMenuOpen = true;
        actorMenuJustOpened = true;
        actorMenuPos = mousePos;
        float relX = (mousePos.x - graphArea.x - graphArea.width  * 0.5f) * 0.05f * orbitRadius;
        float relZ = (mousePos.y - graphArea.y - graphArea.height * 0.5f) * 0.05f * orbitRadius;
        actorMenuWorldPos = {relX, relZ};
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !actorMenuOpen) {
        selectedActorIndex = -1;

        Camera3D projCam = camera;
        const auto& instances = starMap.GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};
            Vector2 fboPos = GetWorldToScreen(pos, projCam);
            Vector2 windowPos = {fboPos.x + graphArea.x, fboPos.y + graphArea.y};
            if (CheckCollisionPointCircle(mousePos, windowPos, 18.0f)) {
                selectedActorIndex = (int)i;
                break;
            }
        }
    }

    if (IsKeyPressed(KEY_DELETE) && selectedActorIndex >= 0) {
        starMap.RemoveInstance(selectedActorIndex);
        selectedActorIndex = -1;
        mapDirty = true;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && mapDirty) SaveMap();
}

void ViewportPanel::RenderActorContextMenu() {
    if (!actorMenuOpen) return;

    static const char* classes[] = {"Actor", "Pawn", "Character"};
    float menuW = 130;
    float itemH = 22;
    float menuH = 8 + (float)(3 * itemH) + 8;
    Rectangle popup = {actorMenuPos.x, actorMenuPos.y, menuW, menuH};

    if (actorMenuJustOpened) {
        actorMenuJustOpened = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), popup)) {
        actorMenuOpen = false;
        return;
    }

    DrawRectangleRec(popup, {50, 50, 55, 255});
    DrawRectangleLinesEx(popup, 1, {80, 80, 90, 255});

    float y = popup.y + 6;
    DrawText("Place Actor", (int)popup.x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 16;

    for (int i = 0; i < 3; i++) {
        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, btn)) {
            DrawRectangleRec(btn, {70, 70, 85, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ActorInstance inst;
                inst.name = std::string(classes[i]) + "_" + std::to_string(starMap.GetInstances().size() + 1);
                inst.className = classes[i];
                inst.posX = actorMenuWorldPos.x;
                inst.posY = 0;
                inst.posZ = actorMenuWorldPos.y;
                starMap.AddInstance(inst);
                selectedActorIndex = (int)starMap.GetInstances().size() - 1;
                mapDirty = true;
                actorMenuOpen = false;
            }
        }
        DrawText(classes[i], (int)btn.x + 8, (int)btn.y + 4, 12, WHITE);
        y += itemH;
    }
}

void ViewportPanel::RenderToolbar() {
    Rectangle toolbar = {bounds.x, bounds.y + 25, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, {36, 36, 40, 255});
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), {60, 60, 65, 255});

    DrawText(name, (int)bounds.x + 6, (int)bounds.y + 30, 13, WHITE);

    float btnRight = bounds.x + bounds.width - 4;
    float btnY = toolbar.y + 3;

    if (!playing) {
        Rectangle playBtn = {btnRight - 22, btnY, 22, 22};
        DrawRectangleRec(playBtn, {30, 130, 60, 255});
        DrawText(">", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), playBtn)) {
            playing = true;
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
            playing = false;
        }
        btnRight -= 26;
    }

    if (mapLoaded) {
        Rectangle saveBtn = {btnRight - 50, btnY, 50, 22};
        if (GuiButton(saveBtn, "Save")) SaveMap();
        btnRight -= 54;

        if (mapDirty) {
            DrawText("*", (int)btnRight - 12, (int)btnY + 4, 14, {200, 160, 60, 255});
        }
    }
}

void ViewportPanel::Render() {
    Rectangle toolbar = {bounds.x, bounds.y + 25, bounds.width, toolbarHeight};
    Rectangle client  = {bounds.x, toolbar.y + toolbarHeight, bounds.width, bounds.height - 25 - toolbarHeight};
    int clientWidth  = (int)client.width;
    int clientHeight = (int)client.height;

    if (clientWidth > 0 && clientHeight > 0) {
        EnsureRenderTarget(clientWidth, clientHeight);
        UpdateCamera(client);
        HandleMapInteraction(client);

        BeginTextureMode(renderTarget);
        ClearBackground({28, 28, 30, 255});
        RenderScene();
        EndTextureMode();

        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        DrawTextureRec(renderTarget.texture, source, Vector2{client.x, client.y}, WHITE);
    }

    DrawRectangleLinesEx(bounds, 1, {70, 70, 75, 255});
    RenderToolbar();
    RenderActorContextMenu();

    if (mapLoaded && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        char info[128];
        snprintf(info, sizeof(info), "%s (%s)", inst.name.c_str(), inst.className.c_str());
        DrawText(info, (int)bounds.x + 8, (int)(bounds.y + 25 + toolbarHeight + 6), 12, {200, 200, 210, 255});
    }

    if (!mapLoaded) {
        const char* msg = "Open a .starmap file from the Content Browser";
        int tw = MeasureText(msg, 13);
        DrawText(msg, (int)(bounds.x + bounds.width / 2 - tw / 2),
                 (int)(bounds.y + bounds.height / 2), 13, {100, 100, 110, 255});
    }
}