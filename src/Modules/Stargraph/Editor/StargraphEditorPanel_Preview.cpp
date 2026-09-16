#include "StargraphEditorPanel.h"
#include "Core/Widgets/Widget.h"
#include "Editor/BasicActorDefs.h"
#include "Editor/UITheme.h"
#include "Editor/UIWidgets.h"
#include "Runtime/Actor.h"
#include "Runtime/AssetCache.h"
#include "Core/StarAsset.h"
#include "Core/EmbeddedAssets.h"
#include "Core/SafeParse.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

const float StargraphEditorPanel::previewToolbarHeight = 28.0f;

StargraphEditorPanel::~StargraphEditorPanel() {
    if (previewRenderTargetWidth != 0) UnloadRenderTexture(previewRenderTarget);
    for (auto& kv : previewIcons) if (kv.second.id != 0) UnloadTexture(kv.second);
    delete leftChrome;
    delete compChrome;
}

static void PreviewUpdateOrbitPosition(Camera3D& camera, float yaw, float pitch, float radius, Vector3 target) {
    float yr = yaw * DEG2RAD;
    float pr = pitch * DEG2RAD;
    camera.position = {
        target.x + cosf(pr) * sinf(yr) * radius,
        target.y + sinf(pr) * radius,
        target.z + cosf(pr) * cosf(yr) * radius
    };
    camera.target = target;
}

static void PreviewComputeDeclWorldTransform(const std::vector<ComponentDecl>& comps, const ComponentDecl& target,
                                              Vector3& outPos, Vector3& outRot, Vector3& outScale) {
    std::vector<const ComponentDecl*> chain;
    const ComponentDecl* cur = &target;
    int guard = 0;
    while (cur && guard++ < 64) {
        chain.push_back(cur);
        if (cur->parentName.empty()) break;
        const ComponentDecl* next = nullptr;
        for (const ComponentDecl& c : comps) {
            if (c.name == cur->parentName) { next = &c; break; }
        }
        cur = next;
    }

    outPos = {0, 0, 0};
    outRot = {0, 0, 0};
    outScale = {1, 1, 1};
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const ComponentDecl* c = *it;
        outPos = Actor::ComputeWorldPosition({c->posX, c->posY, c->posZ}, outPos, outRot, outScale);
        outRot = Actor::ComputeWorldRotation({c->rotX, c->rotY, c->rotZ}, outRot);
        outScale = Actor::ComputeWorldScale({c->scaleX, c->scaleY, c->scaleZ}, outScale);
    }
}

static std::string PreviewIconForComponent(const ComponentDecl& comp) {
    if (comp.componentType == "LightComponent") {
        std::string lightType = comp.Get("LightType", "Point");
        if (lightType == "Sun") return "icon_actor_light_sun.png";
        if (lightType == "Spot") return "icon_actor_light_spot.png";
        if (lightType == "Rectangle") return "icon_actor_light_rect.png";
        if (lightType == "Global") return "icon_actor_light_global.png";
        return "icon_actor_light_point.png";
    }
    if (comp.componentType == "CameraComponent") return "icon_actor_camera.png";
    if (comp.componentType == "AudioComponent") return "icon_actor_audio.png";
    return "icon_actor.png";
}

Texture2D StargraphEditorPanel::GetOrLoadPreviewIcon(const std::string& iconFile) {
    if (iconFile.empty()) return Texture2D{};
    auto it = previewIcons.find(iconFile);
    if (it != previewIcons.end()) return it->second;
    Texture2D tex = EmbeddedAssets::Get().LoadTextureAsset(iconFile);
    previewIcons[iconFile] = tex;
    return tex;
}

void StargraphEditorPanel::EnsurePreviewRenderTarget(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (previewRenderTargetWidth == width && previewRenderTargetHeight == height) return;
    if (previewRenderTargetWidth != 0) UnloadRenderTexture(previewRenderTarget);
    previewRenderTarget = LoadRenderTexture(width, height);
    previewRenderTargetWidth = width;
    previewRenderTargetHeight = height;
}

void StargraphEditorPanel::UpdatePreviewCamera(Rectangle client) {
    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, client) && !previewFlying && !previewOrbiting) return;

    float dt = GetFrameTime();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, client)) {
        previewFlying = true;
        previewLookStart = mp;
        previewFlyPosition = previewCamera.position;
        previewYaw = previewYaw + 180.0f;
        previewPitch = -previewPitch;
        HideCursor();
    }

    if (previewFlying) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = {mp.x - previewLookStart.x, mp.y - previewLookStart.y};
            previewYaw   -= delta.x * 0.3f;
            previewPitch -= delta.y * 0.3f;
            if (previewPitch >  89.0f) previewPitch =  89.0f;
            if (previewPitch < -89.0f) previewPitch = -89.0f;
            SetMousePosition((int)previewLookStart.x, (int)previewLookStart.y);

            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                previewFlySpeed += wheel * 100.0f;
                if (previewFlySpeed < 40.0f)   previewFlySpeed = 40.0f;
                if (previewFlySpeed > 4000.0f) previewFlySpeed = 4000.0f;
            }

            float yr = previewYaw   * DEG2RAD;
            float pr = previewPitch * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            Vector3 right   = {cosf(yr), 0.0f, -sinf(yr)};
            Vector3 up      = {0.0f, 1.0f, 0.0f};

            if (IsKeyDown(KEY_W)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(forward,   previewFlySpeed * dt));
            if (IsKeyDown(KEY_S)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(forward,  -previewFlySpeed * dt));
            if (IsKeyDown(KEY_A)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(right,     previewFlySpeed * dt));
            if (IsKeyDown(KEY_D)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(right,    -previewFlySpeed * dt));
            if (IsKeyDown(KEY_E)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(up,        previewFlySpeed * dt));
            if (IsKeyDown(KEY_Q)) previewFlyPosition = Vector3Add(previewFlyPosition, Vector3Scale(up,       -previewFlySpeed * dt));

            previewCamera.position = previewFlyPosition;
            previewCamera.target   = Vector3Add(previewFlyPosition, forward);
        } else {
            previewFlying = false;
            ShowCursor();
            previewYaw = previewYaw + 180.0f;
            previewPitch = -previewPitch;
            float yr = previewYaw   * DEG2RAD;
            float pr = previewPitch * DEG2RAD;
            Vector3 lookDir = {cosf(-pr) * sinf(yr + 3.14159265f), sinf(-pr), cosf(-pr) * cosf(yr + 3.14159265f)};
            previewCameraTarget = Vector3Add(previewFlyPosition, Vector3Scale(lookDir, previewOrbitRadius));
        }
        return;
    }

    float scroll = GetMouseWheelMove();
    if (scroll != 0 && CheckCollisionPointRec(mp, client)) {
        previewOrbitRadius -= scroll * 100.0f;
        if (previewOrbitRadius < 50.0f) previewOrbitRadius = 50.0f;
    }

    bool altOrbitHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if ((IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
        && CheckCollisionPointRec(mp, client)) {
        previewOrbiting = true;
        previewOrbitStart = mp;
    }
    if (previewOrbiting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
            Vector2 delta = {mp.x - previewOrbitStart.x, mp.y - previewOrbitStart.y};
            previewYaw   -= delta.x * 0.4f;
            previewPitch += delta.y * 0.4f;
            if (previewPitch >  89.0f) previewPitch =  89.0f;
            if (previewPitch < -89.0f) previewPitch = -89.0f;
            previewOrbitStart = mp;
        } else {
            previewOrbiting = false;
        }
    }

    if (IsKeyPressed(KEY_F) && selectedComponentIndex >= 0 && selectedComponentIndex < (int)graph.GetComponents().size()) {
        Vector3 wp, wr, ws;
        PreviewComputeDeclWorldTransform(graph.GetComponents(), graph.GetComponents()[selectedComponentIndex], wp, wr, ws);
        previewCameraTarget = wp;
    }

    PreviewUpdateOrbitPosition(previewCamera, previewYaw, previewPitch, previewOrbitRadius, previewCameraTarget);
}

void StargraphEditorPanel::RenderPreviewScene() {
    rlSetClipPlanes(0.1, 500000.0);
    BeginMode3D(previewCamera);

    DrawGrid(20, 50.0f);

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    Vector3 selectedPos{0, 0, 0};
    Vector3 selectedRot{0, 0, 0};
    bool haveSelected = false;

    for (size_t i = 0; i < comps.size(); i++) {
        const ComponentDecl& comp = comps[i];
        bool selected = (selectedComponentIndex == (int)i);
        Vector3 pos, rot, scl;
        PreviewComputeDeclWorldTransform(comps, comp, pos, rot, scl);

        if (comp.componentType == "MeshComponent") {
            std::string meshAssetPath = comp.Get("Mesh");
            Model* model = nullptr;
            if (!meshAssetPath.empty()) {
                StarAsset asset;
                if (StarAsset::LoadFromFile(meshAssetPath, asset) && asset.assetType == "StaticMesh") {
                    model = AssetCache::Get().GetOrLoadModel(asset.ResolveSourcePath());
                }
            }
            if (model) {
                Matrix scaleMatrix = MatrixScale(scl.x, scl.y, scl.z);
                Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
                model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);
                auto materialIt = comp.arrayFields.find("Material");
                static const std::vector<std::string> emptyMaterials;
                AssetCache::ApplyMaterialSlots(model, materialIt != comp.arrayFields.end() ? materialIt->second : emptyMaterials);
                DrawModel(*model, pos, 1.0f, WHITE);
                if (selected) DrawModelWires(*model, pos, 1.0f, {255, 200, 50, 255});
            } else {
                Texture2D tex = GetOrLoadPreviewIcon("icon_actor_mesh.png");
                if (tex.id != 0) DrawBillboardRec(previewCamera, tex, {0, 0, (float)tex.width, (float)tex.height}, pos, {40, 40}, WHITE);
            }
        } else if (comp.componentType == "CollisionComponent") {
            if (selected) {
                rlPushMatrix();
                rlTranslatef(pos.x, pos.y, pos.z);
                rlRotatef(rot.y, 0, 1, 0);
                rlRotatef(rot.x, 1, 0, 0);
                rlRotatef(rot.z, 0, 0, 1);
                Color c = {90, 200, 255, 200};
                std::string collisionShape = comp.Get("CollisionShape", "Box");
                float collisionRadius = SafeParse::ToFloat(comp.Get("CollisionRadius", "50"));
                float collisionHeight = SafeParse::ToFloat(comp.Get("CollisionHeight", "100"));
                if (collisionShape == "Sphere") {
                    DrawSphereWires({0, 0, 0}, collisionRadius, 8, 12, c);
                } else if (collisionShape == "Capsule") {
                    float half = collisionHeight * 0.5f;
                    DrawCylinderWires({0, -half, 0}, collisionRadius, collisionRadius, collisionHeight, 12, c);
                    DrawSphereWires({0, half, 0}, collisionRadius, 8, 12, c);
                    DrawSphereWires({0, -half, 0}, collisionRadius, 8, 12, c);
                } else {
                    DrawCubeWiresV({0, 0, 0}, {comp.scaleX, comp.scaleY, comp.scaleZ}, c);
                }
                rlPopMatrix();
            }
        } else if (comp.componentType == "BillboardComponent") {
            std::string billboardTexturePath = comp.Get("BillboardTexture");
            Texture2D* tex = billboardTexturePath.empty() ? nullptr : AssetCache::Get().GetOrLoadTexture(billboardTexturePath);
            float billboardWidth = SafeParse::ToFloat(comp.Get("BillboardWidth", "50"));
            float billboardHeight = SafeParse::ToFloat(comp.Get("BillboardHeight", "50"));
            if (tex) DrawBillboardRec(previewCamera, *tex, {0, 0, (float)tex->width, (float)tex->height}, pos, {billboardWidth, billboardHeight}, WHITE);
        } else {
            std::string iconFile = PreviewIconForComponent(comp);
            Texture2D tex = GetOrLoadPreviewIcon(iconFile);
            if (tex.id != 0) DrawBillboardRec(previewCamera, tex, {0, 0, (float)tex.width, (float)tex.height}, pos, {40, 40}, WHITE);
        }

        if (selected) {
            DrawCircle3D(pos, 20.0f, {1, 0, 0}, 90.0f, {255, 200, 50, 255});
            selectedPos = pos;
            selectedRot = rot;
            haveSelected = true;
        }
    }

    if (haveSelected) {
        GizmoRenderer::Render(previewCamera, previewRenderTargetWidth, previewRenderTargetHeight,
                               previewGizmoMode, previewGizmoSpace, selectedPos, selectedRot, previewGizmoState);
    }

    EndMode3D();
}

void StargraphEditorPanel::HandlePreviewInteraction(Rectangle client) {
    if (IsKeyPressed(KEY_F11) && onTogglePreviewFullscreen) onTogglePreviewFullscreen();

    std::vector<ComponentDecl>& comps = graph.GetComponents();

    bool gizmoActive = false;
    bool selectionValid = selectedComponentIndex >= 0 && selectedComponentIndex < (int)comps.size();
    Vector3 gizmoPos{0, 0, 0}, gizmoRot{0, 0, 0}, gizmoScale{1, 1, 1};
    if (selectionValid) {
        ComponentDecl& comp = comps[(size_t)selectedComponentIndex];
        gizmoPos = {comp.posX, comp.posY, comp.posZ};
        gizmoRot = {comp.rotX, comp.rotY, comp.rotZ};
        gizmoScale = {comp.scaleX, comp.scaleY, comp.scaleZ};
    }
    bool changed = false;
    gizmoActive = GizmoRenderer::UpdateDrag(previewCamera, previewRenderTargetWidth, previewRenderTargetHeight,
                                             previewGizmoMode, previewGizmoSpace, client, false, 1.0f, selectionValid,
                                             gizmoPos, gizmoRot, gizmoScale, previewGizmoState, changed);
    if (changed && selectionValid) {
        ComponentDecl& comp = comps[(size_t)selectedComponentIndex];
        comp.posX = gizmoPos.x; comp.posY = gizmoPos.y; comp.posZ = gizmoPos.z;
        comp.rotX = gizmoRot.x; comp.rotY = gizmoRot.y; comp.rotZ = gizmoRot.z;
        comp.scaleX = gizmoScale.x; comp.scaleY = gizmoScale.y; comp.scaleZ = gizmoScale.z;
        isDirty = true;
    }

    Vector2 mp = GetMousePosition();
    if (!CheckCollisionPointRec(mp, client) && previewGizmoState.dragAxis == -1) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !gizmoActive) {
        Vector2 localMp = {mp.x - client.x, mp.y - client.y};
        int hit = -1;
        for (size_t i = 0; i < comps.size(); i++) {
            Vector3 pos, rot, scl;
            PreviewComputeDeclWorldTransform(comps, comps[i], pos, rot, scl);
            Vector2 screen = GizmoRenderer::WorldToScreenPoint(previewCamera, previewRenderTargetWidth, previewRenderTargetHeight, pos);
            if (CheckCollisionPointCircle(localMp, screen, 20.0f)) { hit = (int)i; break; }
        }
        selectedComponentIndex = hit;
        lastValidComponentIndex = hit;
    }

    if (!previewFlying) {
        if (IsKeyPressed(KEY_Q)) previewGizmoMode = GizmoMode::Select;
        if (IsKeyPressed(KEY_W)) previewGizmoMode = GizmoMode::Move;
        if (IsKeyPressed(KEY_E)) previewGizmoMode = GizmoMode::Rotate;
        if (IsKeyPressed(KEY_R)) previewGizmoMode = GizmoMode::Scale;
    }
}

void StargraphEditorPanel::RenderPreviewToolbar(Rectangle toolbarArea) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(toolbarArea);

    float bx = toolbarArea.x + 6;
    float by = toolbarArea.y + 3;
    float bs = 22;

    auto iconLoader = [this](const std::string& f) { return GetOrLoadPreviewIcon(f); };

    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_select.png", previewGizmoMode == GizmoMode::Select, iconLoader)) previewGizmoMode = GizmoMode::Select;
    bx += bs + 2;
    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_move.png", previewGizmoMode == GizmoMode::Move, iconLoader)) previewGizmoMode = GizmoMode::Move;
    bx += bs + 2;
    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_rotate.png", previewGizmoMode == GizmoMode::Rotate, iconLoader)) previewGizmoMode = GizmoMode::Rotate;
    bx += bs + 2;
    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_scale.png", previewGizmoMode == GizmoMode::Scale, iconLoader)) previewGizmoMode = GizmoMode::Scale;
    bx += bs + 10;

    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_world.png", previewGizmoSpace == GizmoSpace::World, iconLoader)) previewGizmoSpace = GizmoSpace::World;
    bx += bs + 2;
    if (GizmoRenderer::ToggleButton({bx, by, bs, bs}, "icon_gizmo_local.png", previewGizmoSpace == GizmoSpace::Local, iconLoader)) previewGizmoSpace = GizmoSpace::Local;

    Rectangle fullscreenBtn = {toolbarArea.x + toolbarArea.width - bs - 4, by, bs, bs};
    if (GizmoRenderer::ToggleButton(fullscreenBtn, "icon_panel_fullscreen.png", previewFullscreen, iconLoader)) {
        if (onTogglePreviewFullscreen) onTogglePreviewFullscreen();
    }
}

void StargraphEditorPanel::RenderPreviewViewport(Rectangle area) {
    Rectangle toolbarArea = {area.x, area.y, area.width, previewToolbarHeight};
    Rectangle client = {area.x, area.y + previewToolbarHeight, area.width, area.height - previewToolbarHeight};

    RenderPreviewToolbar(toolbarArea);

    EnsurePreviewRenderTarget((int)client.width, (int)client.height);
    if (previewRenderTargetWidth == 0) return;

    UpdatePreviewCamera(client);
    HandlePreviewInteraction(client);

    BeginTextureMode(previewRenderTarget);
    ClearBackground({35, 35, 40, 255});
    RenderPreviewScene();
    EndTextureMode();

    DrawTexturePro(previewRenderTarget.texture,
        {0, 0, (float)previewRenderTarget.texture.width, -(float)previewRenderTarget.texture.height},
        client, {0, 0}, 0, WHITE);
}
