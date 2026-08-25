#include "ViewportPanel.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/CameraComponent.h"
#include "Modules/Stargraph/Nodes_Collision.h"
#include "Runtime/AssetCache.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/PrintMessageLog.h"
#include "Core/RenderPipeline.h"
#include "Core/RenderSettings.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/ContentPathResolver.h"
#include "Core/EmbeddedAssets.h"
#include "LogPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raymath.h"
#include "rlgl.h"
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <algorithm>

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
    if (pieRenderTarget.id != 0) UnloadRenderTexture(pieRenderTarget);
    for (auto& kv : editorIcons) if (kv.second.id != 0) UnloadTexture(kv.second);
}

const Stargraph* ViewportPanel::GetOrLoadClassGraph(const std::string& graphAssetPath) {
    return StargraphAssetCache::Get().GetOrLoad(graphAssetPath);
}

void ViewportPanel::InvalidateClassGraph(const std::string& graphAssetPath) {
    StargraphAssetCache::Get().Invalidate(graphAssetPath);
}

void ViewportPanel::DrawInstanceOrMesh(const std::string& meshAssetPath, const std::vector<std::string>& materialAssetPaths,
                                        Vector3 pos, Vector3 rot, Vector3 scale, bool selected, Color fallbackColor) {
    Model* model = nullptr;
    if (!meshAssetPath.empty()) {
        StarAsset asset;
        if (StarAsset::LoadFromFile(meshAssetPath, asset) && asset.assetType == "StaticMesh") {
            model = AssetCache::Get().GetOrLoadModel(asset.ResolveSourcePath());
        }
    }

    if (model) {
        Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
        Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
        model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);

        AssetCache::ApplyMaterialSlots(model, materialAssetPaths);
        DrawModel(*model, pos, 1.0f, WHITE);
        if (selected) DrawModelWires(*model, pos, 1.0f, {255, 200, 50, 255});
    } else if (!meshAssetPath.empty()) {
        // A mesh is referenced but couldn't be resolved.
        DrawCube(pos, scale.x, scale.y, scale.z, fallbackColor);
        DrawCubeWires(pos, scale.x, scale.y, scale.z, {0, 0, 0, 120});
        DrawCubeWires(pos, scale.x * 1.08f, scale.y * 1.08f, scale.z * 1.08f, {220, 40, 200, 200});
    } else {
        DrawActorBillboard("icon_actor_mesh.png", pos, WHITE);
    }
}

void ViewportPanel::DrawCollisionShape(const ComponentDecl& comp, Vector3 pos, Vector3 rot, Color color) {
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(rot.y, 0, 1, 0);
    rlRotatef(rot.x, 1, 0, 0);
    rlRotatef(rot.z, 0, 0, 1);

    if (comp.collisionShape == "Sphere") {
        DrawSphereWires({0, 0, 0}, comp.collisionRadius, 8, 12, color);
    } else if (comp.collisionShape == "Capsule") {
        float half = comp.collisionHeight * 0.5f;
        DrawCylinderWires({0, -half, 0}, comp.collisionRadius, comp.collisionRadius, comp.collisionHeight, 12, color);
        DrawSphereWires({0, half, 0}, comp.collisionRadius, 8, 12, color);
        DrawSphereWires({0, -half, 0}, comp.collisionRadius, 8, 12, color);
    } else {
        DrawCubeWiresV({0, 0, 0}, {comp.scaleX, comp.scaleY, comp.scaleZ}, color);
    }

    rlPopMatrix();
}

Texture2D ViewportPanel::GetOrLoadEditorIcon(const std::string& iconFile) {
    if (iconFile.empty()) return Texture2D{};
    auto it = editorIcons.find(iconFile);
    if (it != editorIcons.end()) return it->second;
    Texture2D tex = EmbeddedAssets::Get().LoadTextureAsset(iconFile);
    editorIcons[iconFile] = tex;
    return tex;
}

void ViewportPanel::DrawActorBillboard(const std::string& iconFile, Vector3 pos, Color tint) {
    Texture2D tex = GetOrLoadEditorIcon(iconFile);
    if (tex.id == 0) return;
    const float billboardSize = 40.0f;
    DrawBillboardRec(camera, tex, {0, 0, (float)tex.width, (float)tex.height}, pos, {billboardSize, billboardSize}, tint);
}

void ViewportPanel::EnsureRenderTarget(int width, int height) {
    if (width == renderTargetWidth && height == renderTargetHeight) return;
    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    renderTarget = LoadRenderTexture(width, height);
    renderTargetWidth = width;
    renderTargetHeight = height;
}

void ViewportPanel::EnsurePieRenderTarget(int width, int height) {
    if (width == pieRenderTargetWidth && height == pieRenderTargetHeight) return;
    if (pieRenderTarget.id != 0) UnloadRenderTexture(pieRenderTarget);
    pieRenderTarget = LoadRenderTexture(width, height);
    SetTextureFilter(pieRenderTarget.texture, TEXTURE_FILTER_POINT);
    pieRenderTargetWidth = width;
    pieRenderTargetHeight = height;
}

void ViewportPanel::LoadMap(const std::string& path) {
    starMap.LoadFromFile(path);
    mapPath = path;
    mapLoaded = true;
    mapDirty = false;
    selectedActorIndex = -1;
    StargraphAssetCache::Get().Clear();
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
    inst.graphAssetPath = ContentPathResolver::MakeRelative(assetPath);
    inst.posX = worldX;
    inst.posY = 0;
    inst.posZ = worldZ;
    inst.scaleX = inst.scaleY = inst.scaleZ = 1.0f;

    starMap.AddInstance(inst);
    selectedActorIndex = (int)starMap.GetInstances().size() - 1;
    mapDirty = true;
}

void ViewportPanel::DropStaticMeshAsset(const std::string& assetPath, Vector2 screenPos) {
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

    StarAsset asset;
    StarAsset::LoadFromFile(assetPath, asset);
    fs::path src = asset.ResolveSourcePath().empty() ? fs::path(assetPath) : fs::path(asset.ResolveSourcePath());
    std::string baseName = src.stem().string();

    ActorInstance inst;
    inst.name = baseName + "_" + std::to_string(starMap.GetInstances().size() + 1);
    inst.className = "StaticMeshActor";
    inst.meshAssetPath = ContentPathResolver::MakeRelative(assetPath);
    inst.actorKind = "MeshActor";
    inst.posX = worldX + asset.importPosX;
    inst.posY = asset.importPosY;
    inst.posZ = worldZ + asset.importPosZ;
    inst.rotX = asset.importRotX;
    inst.rotY = asset.importRotY;
    inst.rotZ = asset.importRotZ;
    
    inst.scaleX = asset.importScaleX;
    inst.scaleY = asset.importScaleY;
    inst.scaleZ = asset.importScaleZ;
    for (const MaterialSlot& slot : asset.materialSlots) inst.materialAssetPaths.push_back(slot.materialPath);

    starMap.AddInstance(inst);
    selectedActorIndex = (int)starMap.GetInstances().size() - 1;
    mapDirty = true;
}

void ViewportPanel::DropBasicActor(const BasicActorDef& def, Vector2 screenPos) {
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

    ActorInstance inst;
    inst.name = def.label + "_" + std::to_string(starMap.GetInstances().size() + 1);
    inst.posX = worldX;
    inst.posY = 0;
    inst.posZ = worldZ;
    inst.scaleX = inst.scaleY = inst.scaleZ = 1.0f;

    if (def.isMeshActor) {
        inst.className = "StaticMeshActor";
    } else {
        inst.className = "Actor";
        inst.hasComponent = true;
        inst.component = def.defaultComponent;
    }
    inst.actorKind = def.kindName;

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

    bool altOrbitHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if ((IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
        && CheckCollisionPointRec(mp, client) && !playing) {
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

    if (IsKeyPressed(KEY_F) && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        cameraTarget = {inst.posX, inst.posY, inst.posZ};
    }

    if (playing) {
        const auto& actors = World::Get().GetActors();
        if (!actors.empty()) {
            Actor* camActor = World::Get().GetViewTarget();
            if (!camActor) {
                for (const auto& actor : actors) {
                    if (actor->GetCameraComponent()) {
                        camActor = actor.get();
                        World::Get().SetViewTarget(camActor);
                        break;
                    }
                }
            }
            if (camActor) {
                CameraComponent* cc = camActor->GetCameraComponent();
                Vector3 compPos = camActor->GetComponentWorldPosition(cc);
                Vector3 compRot = camActor->GetComponentWorldRotation(cc);
                float yr = compRot.y * DEG2RAD;
                float pr = compRot.x * DEG2RAD;
                Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
                camera.position = compPos;
                camera.target = Vector3Add(compPos, forward);
                camera.up = {0, 1, 0};
                camera.fovy = cc->fov;
                camera.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
                rlSetClipPlanes((double)cc->nearClip, (double)cc->farClip);
            } else {
                Actor* first = actors.front().get();
                cameraTarget = first->position;
            }
        }
    }

    if (!playing || World::Get().GetActors().empty() || !World::Get().GetViewTarget()) {
        UpdateOrbitPosition(camera, yaw, pitch, orbitRadius, cameraTarget);
    }
}

static Color ClassColor(const std::string& className) {
    return {200, 160, 80, 255};
}

static std::string IconForActorKind(const std::string& kind) {
    for (const BasicActorDef& def : GetBasicActorDefs()) {
        if (def.kindName == kind) return def.iconFile;
    }
    return "icon_actor.png";
}

static void ComputeDeclWorldTransform(const std::vector<ComponentDecl>& comps, const ComponentDecl& target,
                                       Vector3 basePos, Vector3 baseRot, Vector3 baseScale,
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

    outPos = basePos;
    outRot = baseRot;
    outScale = baseScale;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const ComponentDecl* c = *it;
        outPos = Actor::ComputeWorldPosition({c->posX, c->posY, c->posZ}, outPos, outRot, outScale);
        outRot = Actor::ComputeWorldRotation({c->rotX, c->rotY, c->rotZ}, outRot);
        outScale = Actor::ComputeWorldScale({c->scaleX, c->scaleY, c->scaleZ}, outScale);
    }
}

static void BuildOrthonormalBasis(Vector3 n, Vector3& u, Vector3& v) {
    Vector3 helper = (fabsf(n.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    u = Vector3Normalize(Vector3CrossProduct(helper, n));
    v = Vector3CrossProduct(n, u);
}

static float DistancePointToSegment2D(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = {b.x - a.x, b.y - a.y};
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (len2 > 0.0001f) {
        t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    Vector2 proj = {a.x + ab.x * t, a.y + ab.y * t};
    return Vector2Distance(p, proj);
}

static Color BrightenColor(Color c) {
    auto lift = [](unsigned char v) { return (unsigned char)std::min(255, (int)v + 70); };
    return {lift(c.r), lift(c.g), lift(c.b), 255};
}

Vector2 ViewportPanel::WorldToScreenPoint(Vector3 worldPos) const {
    Vector2 fboSize = {(float)renderTargetWidth, (float)renderTargetHeight};
    Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD, fboSize.x / fboSize.y, 1.0f, 50000.0f);
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix mvp  = MatrixMultiply(view, proj);

    Vector4 clip = {
        worldPos.x * mvp.m0 + worldPos.y * mvp.m4 + worldPos.z * mvp.m8  + mvp.m12,
        worldPos.x * mvp.m1 + worldPos.y * mvp.m5 + worldPos.z * mvp.m9  + mvp.m13,
        worldPos.x * mvp.m2 + worldPos.y * mvp.m6 + worldPos.z * mvp.m10 + mvp.m14,
        worldPos.x * mvp.m3 + worldPos.y * mvp.m7 + worldPos.z * mvp.m11 + mvp.m15
    };

    if (clip.w <= 0.0001f) return {-99999.0f, -99999.0f};

    Vector2 ndc = {clip.x / clip.w, clip.y / clip.w};
    return { (ndc.x + 1.0f) * 0.5f * fboSize.x, (1.0f - ndc.y) * 0.5f * fboSize.y };
}

float ViewportPanel::GetWorldPerPixel(Vector3 pos) const {
    float dist = Vector3Distance(camera.position, pos);
    return dist * tanf(camera.fovy * 0.5f * DEG2RAD) / ((float)renderTargetHeight * 0.5f);
}

float ViewportPanel::GetGizmoWorldScale(Vector3 pos) const {
    const float desiredPixels = 90.0f;
    return desiredPixels * GetWorldPerPixel(pos);
}

void ViewportPanel::GetGizmoAxes(Vector3 rot, Vector3& axisX, Vector3& axisY, Vector3& axisZ) const {
    if (gizmoSpace == GizmoSpace::World) {
        axisX = {1, 0, 0};
        axisY = {0, 1, 0};
        axisZ = {0, 0, 1};
        return;
    }

    Matrix rotMat = MatrixRotateXYZ({rot.x * DEG2RAD, rot.y * DEG2RAD, rot.z * DEG2RAD});
    axisX = Vector3Transform({1, 0, 0}, rotMat);
    axisY = Vector3Transform({0, 1, 0}, rotMat);
    axisZ = Vector3Transform({0, 0, 1}, rotMat);
}

void ViewportPanel::RenderGizmo(Vector3 pos, Vector3 rot) {
    if (gizmoMode == GizmoMode::Select) return;

    Vector3 axisX, axisY, axisZ;
    GetGizmoAxes(rot, axisX, axisY, axisZ);
    Vector3 axes[3] = {axisX, axisY, axisZ};
    Color colors[3] = { {230, 60, 60, 255}, {70, 210, 90, 255}, {70, 130, 240, 255} };
    float len = GetGizmoWorldScale(pos);
    float worldPerPixel = GetWorldPerPixel(pos);

    rlDrawRenderBatchActive();
    rlDisableDepthTest();

    if (gizmoMode == GizmoMode::Move) {
        for (int a = 0; a < 3; a++) {
            bool active = (gizmoHoverAxis == a || gizmoDragAxis == a);
            float shaftLen = len * 0.75f;
            float tipLen = len * 0.25f;
            float shaftRadius = (active ? 4.0f : 2.5f) * worldPerPixel;
            float tipRadius = (active ? 12.0f : 8.0f) * worldPerPixel;
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 shaftEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen));
            Vector3 tipEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen + tipLen));

            DrawCylinderEx(pos, shaftEnd, shaftRadius, shaftRadius, 8, c);
            DrawCylinderEx(shaftEnd, tipEnd, tipRadius, 0.0f, 12, c);
        }
    } else if (gizmoMode == GizmoMode::Scale) {
        for (int a = 0; a < 3; a++) {
            bool active = (gizmoHoverAxis == a || gizmoDragAxis == a);
            float shaftLen = len * 0.8f;
            float shaftRadius = (active ? 4.0f : 2.5f) * worldPerPixel;
            float cubeSize = (active ? 16.0f : 11.0f) * worldPerPixel;
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 shaftEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen));
            DrawCylinderEx(pos, shaftEnd, shaftRadius, shaftRadius, 8, c);
            DrawCube(shaftEnd, cubeSize, cubeSize, cubeSize, c);
        }
    } else if (gizmoMode == GizmoMode::Rotate) {
        const int segments = 32;
        for (int a = 0; a < 3; a++) {
            bool active = (gizmoHoverAxis == a || gizmoDragAxis == a);
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 u, v;
            BuildOrthonormalBasis(axes[a], u, v);
            Vector3 prevPoint = Vector3Add(pos, Vector3Scale(u, len));
            for (int s = 1; s <= segments; s++) {
                float t = (float)s / (float)segments * 2.0f * PI;
                Vector3 pt = Vector3Add(pos, Vector3Add(Vector3Scale(u, cosf(t) * len), Vector3Scale(v, sinf(t) * len)));
                DrawLine3D(prevPoint, pt, c);
                prevPoint = pt;
            }
        }
    }

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

int ViewportPanel::PickGizmoHandle(Vector2 localMp, Vector3 pos, Vector3 rot) const {
    if (gizmoMode == GizmoMode::Select) return -1;

    Vector3 axisX, axisY, axisZ;
    GetGizmoAxes(rot, axisX, axisY, axisZ);
    Vector3 axes[3] = {axisX, axisY, axisZ};
    float len = GetGizmoWorldScale(pos);

    float bestDist = 12.0f;
    int bestAxis = -1;

    if (gizmoMode == GizmoMode::Rotate) {
        const int segments = 32;
        for (int a = 0; a < 3; a++) {
            Vector3 u, v;
            BuildOrthonormalBasis(axes[a], u, v);
            Vector2 prevScreen = WorldToScreenPoint(Vector3Add(pos, Vector3Scale(u, len)));
            for (int s = 1; s <= segments; s++) {
                float t = (float)s / (float)segments * 2.0f * PI;
                Vector3 pt = Vector3Add(pos, Vector3Add(Vector3Scale(u, cosf(t) * len), Vector3Scale(v, sinf(t) * len)));
                Vector2 ptScreen = WorldToScreenPoint(pt);
                float d = DistancePointToSegment2D(localMp, prevScreen, ptScreen);
                if (d < bestDist) { bestDist = d; bestAxis = a; }
                prevScreen = ptScreen;
            }
        }
    } else {
        Vector2 originScreen = WorldToScreenPoint(pos);
        for (int a = 0; a < 3; a++) {
            Vector2 tipScreen = WorldToScreenPoint(Vector3Add(pos, Vector3Scale(axes[a], len)));
            float d = DistancePointToSegment2D(localMp, originScreen, tipScreen);
            if (d < bestDist) { bestDist = d; bestAxis = a; }
        }
    }

    return bestAxis;
}

bool ViewportPanel::UpdateGizmoDrag(Rectangle client) {
    if (gizmoMode == GizmoMode::Select || selectedActorIndex < 0 ||
        selectedActorIndex >= (int)starMap.GetInstances().size()) {
        gizmoHoverAxis = -1;
        gizmoDragAxis = -1;
        return false;
    }

    Vector2 mp = GetMousePosition();
    Vector2 localMp = {mp.x - client.x, mp.y - client.y};
    ActorInstance& inst = starMap.GetInstances()[(size_t)selectedActorIndex];
    Vector3 pos = {inst.posX, inst.posY, inst.posZ};
    Vector3 rot = {inst.rotX, inst.rotY, inst.rotZ};

    if (gizmoDragAxis == -1) {
        gizmoHoverAxis = CheckCollisionPointRec(mp, client) ? PickGizmoHandle(localMp, pos, rot) : -1;

        if (gizmoHoverAxis != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            gizmoDragAxis = gizmoHoverAxis;
            gizmoDragStartMouse = localMp;
            gizmoDragStartPos = pos;
            gizmoDragStartRot = rot;
            gizmoDragStartScale = {inst.scaleX, inst.scaleY, inst.scaleZ};
            gizmoDragHandleLen = GetGizmoWorldScale(pos);
            gizmoDragWorldPerPixel = GetWorldPerPixel(pos);

            Vector3 axisX, axisY, axisZ;
            GetGizmoAxes(rot, axisX, axisY, axisZ);
            Vector3 axes[3] = {axisX, axisY, axisZ};
            Vector2 originScreen = WorldToScreenPoint(pos);
            Vector2 tipScreen = WorldToScreenPoint(Vector3Add(pos, Vector3Scale(axes[gizmoDragAxis], gizmoDragHandleLen)));
            Vector2 dir2D = {tipScreen.x - originScreen.x, tipScreen.y - originScreen.y};
            float mag = sqrtf(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
            gizmoDragAxisDir2D = (mag > 0.001f) ? Vector2{dir2D.x / mag, dir2D.y / mag} : Vector2{1, 0};

            if (gizmoMode == GizmoMode::Rotate) {
                gizmoDragStartScreenAngle = atan2f(localMp.y - originScreen.y, localMp.x - originScreen.x);
                Vector3 toCamera = Vector3Normalize(Vector3Subtract(camera.position, pos));
                gizmoDragRotSign = (Vector3DotProduct(toCamera, axes[gizmoDragAxis]) >= 0.0f) ? 1.0f : -1.0f;
            }
            return true;
        }
        return false;
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        gizmoDragAxis = -1;
        return false;
    }

    if (gizmoMode == GizmoMode::Move) {
        Vector2 delta = {localMp.x - gizmoDragStartMouse.x, localMp.y - gizmoDragStartMouse.y};
        float t = delta.x * gizmoDragAxisDir2D.x + delta.y * gizmoDragAxisDir2D.y;
        float worldDelta = t * gizmoDragWorldPerPixel;

        Vector3 axisX, axisY, axisZ;
        GetGizmoAxes(gizmoDragStartRot, axisX, axisY, axisZ);
        Vector3 axes[3] = {axisX, axisY, axisZ};
        Vector3 newPos = Vector3Add(gizmoDragStartPos, Vector3Scale(axes[gizmoDragAxis], worldDelta));
        if (IsKeyDown(KEY_LEFT_CONTROL)) {
            newPos.x = roundf(newPos.x);
            newPos.y = roundf(newPos.y);
            newPos.z = roundf(newPos.z);
        }
        inst.posX = newPos.x;
        inst.posY = newPos.y;
        inst.posZ = newPos.z;
    } else if (gizmoMode == GizmoMode::Scale) {
        Vector2 delta = {localMp.x - gizmoDragStartMouse.x, localMp.y - gizmoDragStartMouse.y};
        float t = delta.x * gizmoDragAxisDir2D.x + delta.y * gizmoDragAxisDir2D.y;
        float worldDelta = t * gizmoDragWorldPerPixel;
        float scaleDelta = worldDelta / gizmoDragHandleLen;

        float newScale[3] = {gizmoDragStartScale.x, gizmoDragStartScale.y, gizmoDragStartScale.z};
        newScale[gizmoDragAxis] += scaleDelta;
        if (IsKeyDown(KEY_LEFT_CONTROL)) newScale[gizmoDragAxis] = roundf(newScale[gizmoDragAxis] / 0.1f) * 0.1f;
        if (newScale[gizmoDragAxis] < 0.01f) newScale[gizmoDragAxis] = 0.01f;
        inst.scaleX = newScale[0];
        inst.scaleY = newScale[1];
        inst.scaleZ = newScale[2];
    } else if (gizmoMode == GizmoMode::Rotate) {
        Vector2 originScreen = WorldToScreenPoint(gizmoDragStartPos);
        float currentAngle = atan2f(localMp.y - originScreen.y, localMp.x - originScreen.x);
        float deltaDeg = gizmoDragRotSign * (gizmoDragStartScreenAngle - currentAngle) * RAD2DEG;

        float newRot[3] = {gizmoDragStartRot.x, gizmoDragStartRot.y, gizmoDragStartRot.z};
        newRot[gizmoDragAxis] += deltaDeg;
        if (IsKeyDown(KEY_LEFT_CONTROL)) newRot[gizmoDragAxis] = roundf(newRot[gizmoDragAxis]);
        inst.rotX = newRot[0];
        inst.rotY = newRot[1];
        inst.rotZ = newRot[2];
    }

    mapDirty = true;
    return true;
}

bool ViewportPanel::GizmoToggleButton(Rectangle r, const std::string& iconFile, bool active) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, r);

    Color bg = active ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.buttonBg);
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, active ? theme.success : theme.buttonBorder);

    Texture2D tex = GetOrLoadEditorIcon(iconFile);
    if (tex.id != 0) {
        Rectangle dst = {r.x + 3, r.y + 3, r.width - 6, r.height - 6};
        DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, dst, {0, 0}, 0, WHITE);
    }

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void ViewportPanel::RenderScene() {
    rlSetClipPlanes(0.1, 5000000.0);
    BeginMode3D(camera);

    DrawGrid(40, 100.0f);

    const auto& instances = starMap.GetInstances();
    for (size_t i = 0; i < instances.size(); i++) {
        const ActorInstance& inst = instances[i];
        Vector3 pos = {inst.posX, inst.posY, inst.posZ};
        Vector3 rot = {inst.rotX, inst.rotY, inst.rotZ};
        Vector3 scl = {inst.scaleX, inst.scaleY, inst.scaleZ};
        bool selected = (selectedActorIndex == (int)i);
        Color fallbackColor = selected ? Color{255, 200, 50, 255} : ClassColor(inst.className);

        if (!inst.graphAssetPath.empty()) {
            const Stargraph* classGraph = GetOrLoadClassGraph(inst.graphAssetPath);
            bool drewAnyMesh = false;
            bool drewBillboard = false;

            if (classGraph) {
                for (const ComponentDecl& comp : classGraph->GetComponents()) {
                    Vector3 compPos, compRot, compScale;
                    ComputeDeclWorldTransform(classGraph->GetComponents(), comp, pos, rot, scl,
                                               compPos, compRot, compScale);

                    if (comp.componentType == "MeshComponent") {
                        DrawInstanceOrMesh(comp.meshAssetPath, comp.materialAssetPaths, compPos, compRot,
                                            compScale, selected, fallbackColor);
                        drewAnyMesh = true;
                    } else if (comp.componentType == "CollisionComponent" && selected) {
                        DrawCollisionShape(comp, compPos, compRot, {90, 200, 255, 200});
                    } else if (comp.componentType == "BillboardComponent") {
                        Texture2D* tex = comp.billboardTexturePath.empty() ? nullptr
                                        : AssetCache::Get().GetOrLoadTexture(comp.billboardTexturePath);
                        if (tex) {
                            DrawBillboardRec(camera, *tex, {0, 0, (float)tex->width, (float)tex->height},
                                              compPos, {comp.billboardWidth, comp.billboardHeight}, WHITE);
                            drewBillboard = true;
                        }
                    }
                }
            }

            if (!drewAnyMesh && !drewBillboard) {
                DrawActorBillboard("icon_actor.png", pos, WHITE);
            }
        } else if (inst.hasComponent) {
            DrawActorBillboard(IconForActorKind(inst.actorKind), pos, WHITE);
        } else {
            DrawInstanceOrMesh(inst.meshAssetPath, inst.materialAssetPaths, pos, rot, scl, selected, fallbackColor);
        }

        if (selected) {
            DrawCircle3D(pos, 30.0f, {1, 0, 0}, 90.0f, {255, 200, 50, 255});
            RenderGizmo(pos, rot);
        }
    }

    DrawAndClearCollisionDebug();

    EndMode3D();
}

void ViewportPanel::HandleMapInteraction(Rectangle client) {
    if (!mapLoaded || flying || orbiting) return;
    Vector2 mp = GetMousePosition();

    bool gizmoActive = playing ? false : UpdateGizmoDrag(client);

    if (!CheckCollisionPointRec(mp, client) && gizmoDragAxis == -1) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !gizmoActive) {
        selectedActorIndex = -1;

        Vector2 localMp = {mp.x - client.x, mp.y - client.y};

        const auto& instances = starMap.GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};
            Vector2 screen = WorldToScreenPoint(pos);

            if (CheckCollisionPointCircle(localMp, screen, 20.0f)) {
                selectedActorIndex = (int)i;
                break;
            }
        }
    }

    if (!playing) {
        if (IsKeyPressed(KEY_Q)) gizmoMode = GizmoMode::Select;
        if (IsKeyPressed(KEY_W)) gizmoMode = GizmoMode::Move;
        if (IsKeyPressed(KEY_E)) gizmoMode = GizmoMode::Rotate;
        if (IsKeyPressed(KEY_R)) gizmoMode = GizmoMode::Scale;
    }

    if (IsKeyPressed(KEY_DELETE) && selectedActorIndex >= 0) {
        starMap.RemoveInstance((size_t)selectedActorIndex);
        selectedActorIndex = -1;
        mapDirty = true;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && mapDirty) SaveMap();
}

void ViewportPanel::RenderToolbar() {
    const UIPalette& theme = UITheme::Get();

    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, theme.tabBar);
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), theme.panelBorder);

    if (mapLoaded && !playing) {
        float bx = bounds.x + 6;
        float by = toolbar.y + 3;
        float bs = 22;

        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_select.png", gizmoMode == GizmoMode::Select)) gizmoMode = GizmoMode::Select;
        bx += bs + 2;
        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_move.png", gizmoMode == GizmoMode::Move)) gizmoMode = GizmoMode::Move;
        bx += bs + 2;
        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_rotate.png", gizmoMode == GizmoMode::Rotate)) gizmoMode = GizmoMode::Rotate;
        bx += bs + 2;
        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_scale.png", gizmoMode == GizmoMode::Scale)) gizmoMode = GizmoMode::Scale;
        bx += bs + 10;

        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_world.png", gizmoSpace == GizmoSpace::World)) gizmoSpace = GizmoSpace::World;
        bx += bs + 2;
        if (GizmoToggleButton({bx, by, bs, bs}, "icon_gizmo_local.png", gizmoSpace == GizmoSpace::Local)) gizmoSpace = GizmoSpace::Local;
    }

    float btnRight = bounds.x + bounds.width - 4;
    float btnY = toolbar.y + 3;

    if (!playing) {
    Rectangle playBtn = {btnRight - 22, btnY, 22, 22};
        DrawRectangleRec(playBtn, theme.success);
        DrawText(">", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), playBtn)) {
            playing = true;
            savedYaw = yaw;
            savedPitch = pitch;
            savedOrbitRadius = orbitRadius;
            savedCameraTarget = cameraTarget;
            if (flying) { flying = false; ShowCursor(); }
            WorldSpawner::SpawnMapActors(starMap);
            World::Get().BeginPlay();
            }
        btnRight -= 26;
        }

        else {
        Rectangle stopBtn = {btnRight - 22, btnY, 22, 22};
            DrawRectangleRec(stopBtn, theme.dangerBg);
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

        if (IsKeyPressed(KEY_ESCAPE)) {
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
        if (UIWidget::Button(saveBtn, "Save")) SaveMap();
        btnRight -= 54;
        if (mapDirty) DrawText("*", (int)btnRight - 10, (int)btnY + 4, 14, {200, 160, 60, 255});

        if (!playing) {
            Rectangle packageBtn = {btnRight - 66, btnY, 66, 22};
            if (UIWidget::Button(packageBtn, "Package")) onPackageRequested();
            btnRight -= 70;
        }
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

        Color clearColor = {28, 28, 30, 255};

        if (playing) {
            float scale = RenderSettings::Get().GetRenderScale();
            int pieWidth = (int)(clientWidth * scale);
            int pieHeight = (int)(clientHeight * scale);
            if (pieWidth < 1) pieWidth = 1;
            if (pieHeight < 1) pieHeight = 1;
            EnsurePieRenderTarget(pieWidth, pieHeight);

            RenderPipeline::Render(camera, pieRenderTarget, clearColor);

            BeginTextureMode(renderTarget);
            Rectangle pieSource = {0, 0, (float)pieRenderTarget.texture.width, -(float)pieRenderTarget.texture.height};
            Rectangle pieDest = {0, 0, (float)clientWidth, (float)clientHeight};
            DrawTexturePro(pieRenderTarget.texture, pieSource, pieDest, {0, 0}, 0.0f, WHITE);
            BeginMode3D(camera);
            DrawAndClearCollisionDebug();
            EndMode3D();
            PrintMessageLog::Get().Draw();
            EndTextureMode();
        } else {
            BeginTextureMode(renderTarget);
            ClearBackground(clearColor);
            RenderScene();
            EndTextureMode();
        }

        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        DrawTextureRec(renderTarget.texture, source, {client.x, client.y}, WHITE);
    }

    const UIPalette& theme = UITheme::Get();
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    if (playing) {
        DrawRectangleLinesEx(client, 3, theme.success);
    }
    RenderToolbar();

    if (!playing && mapLoaded && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        char info[128];
        snprintf(info, sizeof(info), "%s (%s)", inst.name.c_str(), inst.className.c_str());
        DrawText(info, (int)bounds.x + 8, (int)(client.y + 6), 12, theme.textPrimary);
    }

    if (flying) {
        const char* hint = "WASD fly  Q/E up/down  Scroll=speed  Release RMB to exit";
        DrawText(hint, (int)bounds.x + 8, (int)(client.y + client.height - 20), 11, {160, 160, 170, 200});
    }

    if (!mapLoaded) {
        const char* msg = "Open a .starmap file from the Content Browser";
        int tw = MeasureText(msg, 13);
        DrawText(msg, (int)(bounds.x + bounds.width / 2 - tw / 2),
                 (int)(bounds.y + bounds.height / 2), 13, theme.textSecondary);
    }
}
