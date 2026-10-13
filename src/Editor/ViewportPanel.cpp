#include "Core/KeyLayout.h"
#include "Core/RotationMath.h"
#include "ViewportPanel.h"
#include "Core/ProfilerRender.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Modules/Stargraph/Nodes_Collision.h"
#include "Runtime/AssetCache.h"
#include "Runtime/DebugDraw.h"
#include "Runtime/CollisionTrace.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/PrintMessageLog.h"
#include "Core/RenderPipeline.h"
#include "Core/GameSession.h"
#include "Core/Widgets/WidgetRenderer.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/RenderSettings.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/ContentPathResolver.h"
#include "Core/EmbeddedAssets.h"
#include "LogPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetFont.h"
#include "GizmoRenderer.h"
#include "raymath.h"
#include "rlgl.h"
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Core/SafeParse.h"

namespace fs = std::filesystem;

static std::string InstanceProfileLabel(const ActorInstance& inst) {
    std::string label = inst.name.empty() ? std::string("Instance") : inst.name;
    if (!inst.className.empty() && inst.className != label) label += " [" + inst.className + "]";
    return label;
}

const float ViewportPanel::toolbarHeight = 28.0f;

static const Color FpsWarnColor = {225, 170, 60, 255};

void ViewportPanel::EnsureToolbarChrome() {
    if (toolbarChrome) return;

    const UIPalette& theme = UITheme::Get();
    toolbarChrome = new Widget();
    auto* root = new WidgetElement();
    toolbarChrome->SetRoot(root);

    gridStepField = new TextElement();
    gridStepField->editable = true;
    gridStepField->numericOnly = true;
    gridStepField->fontSize = 12.0f;
    gridStepField->backgroundColor = theme.fieldBg;
    gridStepField->borderColor = theme.fieldBorder;
    gridStepField->editingBorderColor = theme.accent;
    gridStepField->onTextCommitted = [this](const std::string& text) {
        float v = (float)atof(text.c_str());
        if (v > 0.001f) gridStepSize = v;
    };

    fpsValueField = new TextElement();
    fpsValueField->editable = false;
    fpsValueField->fontSize = 13.0f;
    fpsValueField->color = theme.textSecondary;
    fpsValueField->visible = false;

    fpsStatsField = new TextElement();
    fpsStatsField->editable = false;
    fpsStatsField->fontSize = 11.0f;
    fpsStatsField->color = theme.textSecondary;
    fpsStatsField->visible = false;

    root->AddChild(gridStepField);
    root->AddChild(fpsValueField);
    root->AddChild(fpsStatsField);
}

void ViewportPanel::ResetFpsStats() {
    fpsSampleTime = 0.0f;
    fpsSampleFrames = 0;
    fpsWindowFps = 0.0f;
    fpsWindowAvgMs = 0.0f;
    fpsSessionTime = 0.0f;
    fpsSessionFrames = 0;
    fpsSessionAvgFps = 0.0f;
    fpsBestFrameMs = 0.0f;
    fpsWorstFrameMs = 0.0f;
    fpsSampleValid = false;
    fpsStatsDirty = true;
}

void ViewportPanel::UpdateFpsStats(float dt) {
    if (dt <= 0.0f) return;

    float frameMs = dt * 1000.0f;
    fpsSampleTime += dt;
    fpsSampleFrames++;
    fpsSessionTime += dt;
    fpsSessionFrames++;
    if (fpsSessionFrames == 1 || frameMs < fpsBestFrameMs) fpsBestFrameMs = frameMs;
    if (fpsSessionFrames == 1 || frameMs > fpsWorstFrameMs) fpsWorstFrameMs = frameMs;
    fpsSessionAvgFps = fpsSessionTime > 0.0f ? (float)fpsSessionFrames / fpsSessionTime : 0.0f;

    if (fpsSampleTime >= 0.5f) {
        float frames = (float)fpsSampleFrames;
        fpsWindowFps = frames / fpsSampleTime;
        fpsWindowAvgMs = (fpsSampleTime / frames) * 1000.0f;
        fpsSampleTime = 0.0f;
        fpsSampleFrames = 0;
        fpsSampleValid = true;
        fpsStatsDirty = true;
    }
}

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
    WidgetInput::PopModal(static_cast<const Panel*>(this), WidgetInput::kEditorDevice);
    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    if (pieRenderTarget.id != 0) UnloadRenderTexture(pieRenderTarget);
    delete toolbarChrome;
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
        bool assetLoaded;
        {
            PROFILE_SCOPE("Load Mesh Asset (file read)");
            assetLoaded = StarAsset::LoadFromFile(meshAssetPath, asset);
        }
        if (assetLoaded && asset.assetType == "StaticMesh") {
            PROFILE_SCOPE("Get Model (cache)");
            model = AssetCache::Get().GetOrLoadModel(asset.ResolveSourcePath());
        }
    }

    if (model) {
        {
            PROFILE_SCOPE("Apply Transform And Materials");
            Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
            Matrix rotationMatrix = ComposeRotationDegrees(rot);
            model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);

            AssetCache::ApplyMaterialSlots(model, materialAssetPaths);
        }
        {
            PROFILE_SCOPE("Draw Model");
            DrawModel(*model, pos, 1.0f, WHITE);
            if (selected) DrawModelWires(*model, pos, 1.0f, {255, 200, 50, 255});
        }
    } else if (!meshAssetPath.empty()) {
        // A mesh is referenced but couldn't be resolved.
        DrawCube(pos, scale.x, scale.y, scale.z, fallbackColor);
        DrawCubeWires(pos, scale.x, scale.y, scale.z, {0, 0, 0, 120});
        DrawCubeWires(pos, scale.x * 1.08f, scale.y * 1.08f, scale.z * 1.08f, {220, 40, 200, 200});
    } else {
        DrawActorBillboard("icon_actor_mesh.png", pos, WHITE);
    }
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

bool ViewportPanel::ToolbarIconToggle(Rectangle r, const std::string& iconFile, bool active) {
    return GizmoRenderer::ToggleButton(r, iconFile, active, [this](const std::string& f) { return GetOrLoadEditorIcon(f); });
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
    if (onMapReplaced) onMapReplaced();
}

void ViewportPanel::CaptureUndoState(std::string& content, std::string& selection) const {
    content = starMap.SerializeToString();
    selection = std::to_string(selectedActorIndex);
}

void ViewportPanel::RestoreUndoState(const std::string& content, const std::string& selection) {
    starMap.LoadFromString(content);
    int index = atoi(selection.c_str());
    selectedActorIndex = (index >= 0 && index < (int)starMap.GetInstances().size()) ? index : -1;
    mapDirty = true;
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mp, client) && !playing && !WidgetInput::IsMouseBlocked()) {
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
    if (scroll != 0 && CheckCollisionPointRec(mp, client) && !playing && !WidgetInput::IsMouseBlocked()) {
        orbitRadius -= scroll * 100.0f;
        if (orbitRadius < 100.0f) orbitRadius = 100.0f;
    }

    bool altOrbitHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if ((IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || (altOrbitHeld && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
        && CheckCollisionPointRec(mp, client) && !playing && !WidgetInput::IsMouseBlocked()) {
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

    if (WidgetInput::ScopeHasKeyboard() && KeyLayout::Pressed('f') && selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size()) {
        const ActorInstance& inst = starMap.GetInstances()[selectedActorIndex];
        cameraTarget = {inst.posX, inst.posY, inst.posZ};
    }

    if (playing) {
        GameSession::UpdateActiveCamera(camera);
        if (!World::Get().GetViewTarget()) {
            const auto& actors = World::Get().GetActors();
            if (!actors.empty()) cameraTarget = actors.front()->position;
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

void ViewportPanel::RenderScene() {
    rlSetClipPlanes(0.1, 5000000.0);
    DebugDraw::viewportHeight = (float)renderTargetHeight;
    BeginMode3D(camera);

    float gridStep = (gridStepSize > 0.001f) ? gridStepSize : 100.0f;
    float snappedGridX = floorf(camera.position.x / gridStep) * gridStep;
    float snappedGridZ = floorf(camera.position.z / gridStep) * gridStep;
    {
        PROFILE_RENDER_SCOPE("Grid");
        rlPushMatrix();
        rlTranslatef(snappedGridX, 0.0f, snappedGridZ);
        DrawGrid(40, gridStep);
        rlPopMatrix();
    }

    const auto& instances = starMap.GetInstances();
    Vector3 selectedPos{0, 0, 0};
    Vector3 selectedRot{0, 0, 0};
    bool haveSelected = false;

    {
        PROFILE_RENDER_SCOPE("Instances");
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            PROFILE_SCOPE_DYN(InstanceProfileLabel(inst));
            PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, InstanceProfileLabel(inst));
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};
            Vector3 rot = {inst.rotX, inst.rotY, inst.rotZ};
            Vector3 scl = {inst.scaleX, inst.scaleY, inst.scaleZ};
            bool selected = (selectedActorIndex == (int)i);
            Color fallbackColor = selected ? Color{255, 200, 50, 255} : ClassColor(inst.className);

            if (!inst.graphAssetPath.empty()) {
                const Stargraph* classGraph = nullptr;
                {
                    PROFILE_SCOPE("Class Graph Lookup");
                    classGraph = GetOrLoadClassGraph(inst.graphAssetPath);
                }
                bool drewAnyMesh = false;
                bool drewBillboard = false;

                if (classGraph) {
                    for (const ComponentDecl& comp : classGraph->GetComponents()) {
                        Vector3 compPos, compRot, compScale;
                        ComputeDeclWorldTransform(classGraph->GetComponents(), comp, pos, rot, scl,
                                                   compPos, compRot, compScale);

                        if (comp.componentType == "MeshComponent") {
                            auto materialIt = comp.arrayFields.find("Material");
                            static const std::vector<std::string> emptyMaterials;
                            DrawInstanceOrMesh(comp.Get("Mesh"), materialIt != comp.arrayFields.end() ? materialIt->second : emptyMaterials,
                                                compPos, compRot, compScale, selected, fallbackColor);
                            drewAnyMesh = true;
                        } else if (comp.componentType == "CollisionComponent" && CollisionComponent::DeclVisibleInEditor(comp)) {
                            PROFILE_SCOPE("Collision Debug Shape");
                            CollisionTrace::Shape shape;
                            if (CollisionComponent::BuildShapeFromDecl(comp, compPos, compRot, compScale, shape)) {
                                CollisionTrace::DrawDebugShape(shape, selected ? Color{255, 200, 50, 255} : Color{90, 200, 255, 230});
                            }
                        } else if (comp.componentType == "BillboardComponent") {
                            std::string billboardTexturePath = comp.Get("BillboardTexture");
                            Texture2D* tex = billboardTexturePath.empty() ? nullptr
                                            : AssetCache::Get().GetOrLoadTexture(billboardTexturePath);
                            if (tex) {
                                float billboardWidth = SafeParse::ToFloat(comp.Get("BillboardWidth", "50"));
                                float billboardHeight = SafeParse::ToFloat(comp.Get("BillboardHeight", "50"));
                                DrawBillboardRec(camera, *tex, {0, 0, (float)tex->width, (float)tex->height},
                                                  compPos, {billboardWidth, billboardHeight}, WHITE);
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
                selectedPos = pos;
                selectedRot = rot;
                haveSelected = true;
            }
        }
    }

    if (haveSelected) {
        PROFILE_RENDER_SCOPE("Gizmo");
        GizmoRenderer::Render(camera, renderTargetWidth, renderTargetHeight, gizmoMode, gizmoSpace, selectedPos, selectedRot, gizmoDragState);
    }

    {
        PROFILE_RENDER_SCOPE("Collision Debug Overlay");
        DrawAndClearCollisionDebug((float)renderTargetHeight);
    }

    EndMode3D();
}

void ViewportPanel::HandleMapInteraction(Rectangle client) {
    if (!mapLoaded || flying || orbiting) return;
    Vector2 mp = GetMousePosition();

    if (IsKeyPressed(KEY_F11) && onToggleFullscreen) onToggleFullscreen();

    bool gizmoActive = false;
    if (!playing) {
        bool selectionValid = selectedActorIndex >= 0 && selectedActorIndex < (int)starMap.GetInstances().size();
        Vector3 gizmoPos{0, 0, 0}, gizmoRot{0, 0, 0}, gizmoScale{1, 1, 1};
        if (selectionValid) {
            ActorInstance& inst = starMap.GetInstances()[(size_t)selectedActorIndex];
            gizmoPos = {inst.posX, inst.posY, inst.posZ};
            gizmoRot = {inst.rotX, inst.rotY, inst.rotZ};
            gizmoScale = {inst.scaleX, inst.scaleY, inst.scaleZ};
        }
        bool changed = false;
        gizmoActive = GizmoRenderer::UpdateDrag(camera, renderTargetWidth, renderTargetHeight, gizmoMode, gizmoSpace,
                                                 client, gridSnapEnabled, gridStepSize, selectionValid,
                                                 gizmoPos, gizmoRot, gizmoScale, gizmoDragState, changed);
        if (changed && selectionValid) {
            ActorInstance& inst = starMap.GetInstances()[(size_t)selectedActorIndex];
            inst.posX = gizmoPos.x; inst.posY = gizmoPos.y; inst.posZ = gizmoPos.z;
            inst.rotX = gizmoRot.x; inst.rotY = gizmoRot.y; inst.rotZ = gizmoRot.z;
            inst.scaleX = gizmoScale.x; inst.scaleY = gizmoScale.y; inst.scaleZ = gizmoScale.z;
            mapDirty = true;
        }
    }

    if (!CheckCollisionPointRec(mp, client) && gizmoDragState.dragAxis == -1) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !gizmoActive && !WidgetInput::IsMouseBlocked()) {
        selectedActorIndex = -1;

        Vector2 localMp = {mp.x - client.x, mp.y - client.y};

        const auto& instances = starMap.GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            const ActorInstance& inst = instances[i];
            Vector3 pos = {inst.posX, inst.posY, inst.posZ};
            Vector2 screen = GizmoRenderer::WorldToScreenPoint(camera, renderTargetWidth, renderTargetHeight, pos);

            if (CheckCollisionPointCircle(localMp, screen, 20.0f)) {
                selectedActorIndex = (int)i;
                break;
            }
        }
    }

    if (!playing && WidgetInput::ScopeHasKeyboard()) {
        if (KeyLayout::Pressed('q')) gizmoMode = GizmoMode::Select;
        if (KeyLayout::Pressed('w')) gizmoMode = GizmoMode::Move;
        if (KeyLayout::Pressed('e')) gizmoMode = GizmoMode::Rotate;
        if (KeyLayout::Pressed('r')) gizmoMode = GizmoMode::Scale;
    }

    if (!playing && WidgetInput::ScopeHasKeyboard() && IsKeyPressed(KEY_DELETE) && selectedActorIndex >= 0) {
        starMap.RemoveInstance((size_t)selectedActorIndex);
        selectedActorIndex = -1;
        mapDirty = true;
    }

    if (WidgetInput::ScopeHasKeyboard() && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && mapDirty) SaveMap();
}

void ViewportPanel::RenderToolbar() {
    const UIPalette& theme = UITheme::Get();

    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, theme.tabBar);
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), theme.panelBorder);

    EnsureToolbarChrome();

    Rectangle toolbarBounds = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
    float dt = GetFrameTime();

    if (playing) {
        UpdateFpsStats(dt);

        float fpsRowY = toolbar.y + 3.0f;
        float fpsAnchorY = fpsRowY + 11.0f;

        gridStepField->visible = false;
        fpsValueField->visible = true;
        fpsValueField->offset = { bounds.x + 8.0f, fpsAnchorY };
        fpsValueField->size = { 128.0f, 22.0f };
        fpsValueField->pivot = { 0.0f, 0.5f };
        fpsValueField->color = fpsSampleValid
            ? (fpsWindowFps >= 55.0f ? theme.success : (fpsWindowFps >= 30.0f ? FpsWarnColor : theme.danger))
            : theme.textSecondary;

        if (fpsStatsDirty) {
            char valueBuf[48];
            char statsBuf[128];
            if (fpsSampleValid) {
                float minFps = fpsWorstFrameMs > 0.0f ? 1000.0f / fpsWorstFrameMs : 0.0f;
                float maxFps = fpsBestFrameMs > 0.0f ? 1000.0f / fpsBestFrameMs : 0.0f;
                snprintf(valueBuf, sizeof(valueBuf), "%d fps (%.2f ms)", (int)(fpsWindowFps + 0.5f), fpsWindowAvgMs);
                snprintf(statsBuf, sizeof(statsBuf), "| min %d fps | max %d fps | avg %.0f fps",
                         (int)(minFps + 0.5f), (int)(maxFps + 0.5f), fpsSessionAvgFps);
            } else {
                snprintf(valueBuf, sizeof(valueBuf), "-- fps");
                statsBuf[0] = '\0';
            }
            fpsValueField->text = valueBuf;
            fpsStatsField->text = statsBuf;

            Font font = WidgetFont::Get();
            Vector2 valueSize = MeasureTextEx(font, valueBuf, fpsValueField->fontSize, fpsValueField->fontSize / 10.0f);
            float statsX = fpsValueField->offset.x + valueSize.x + 10.0f;
            fpsStatsField->offset = { statsX, fpsAnchorY };

            Vector2 statsSize = MeasureTextEx(font, statsBuf, fpsStatsField->fontSize, fpsStatsField->fontSize / 10.0f);
            fpsStatsField->size = { statsSize.x + 4.0f, 22.0f };
            fpsStatsField->pivot = { 0.0f, 0.5f };
            fpsStatsField->visible = fpsSampleValid && statsSize.x > 0.0f &&
                statsX + statsSize.x + 200.0f <= bounds.x + bounds.width;
            fpsStatsDirty = false;
        }
        fpsStatsField->color = theme.textSecondary;

        toolbarChrome->Tick(dt, toolbarBounds, 1.0f);
        toolbarChrome->Draw(toolbarBounds, 1.0f);
    }

    if (mapLoaded && !playing) {
        float bx = bounds.x + 6;
        float by = toolbar.y + 3;
        float bs = 22;

        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_select.png", gizmoMode == GizmoMode::Select)) gizmoMode = GizmoMode::Select;
        bx += bs + 2;
        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_move.png", gizmoMode == GizmoMode::Move)) gizmoMode = GizmoMode::Move;
        bx += bs + 2;
        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_rotate.png", gizmoMode == GizmoMode::Rotate)) gizmoMode = GizmoMode::Rotate;
        bx += bs + 2;
        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_scale.png", gizmoMode == GizmoMode::Scale)) gizmoMode = GizmoMode::Scale;
        bx += bs + 10;

        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_world.png", gizmoSpace == GizmoSpace::World)) gizmoSpace = GizmoSpace::World;
        bx += bs + 2;
        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_local.png", gizmoSpace == GizmoSpace::Local)) gizmoSpace = GizmoSpace::Local;
        bx += bs + 10;

        if (ToolbarIconToggle({bx, by, bs, bs}, "icon_gizmo_grid_snap.png", gridSnapEnabled)) gridSnapEnabled = !gridSnapEnabled;
        bx += bs + 4;

        Rectangle stepFieldRect = {bx, by + 2, 56, bs - 4};
        if (!gridStepField->isEditing) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", gridStepSize);
            gridStepField->text = buf;
        }
        gridStepField->size = { stepFieldRect.width, stepFieldRect.height };
        gridStepField->offset = { stepFieldRect.x, stepFieldRect.y };
        gridStepField->visible = true;
        fpsValueField->visible = false;
        fpsStatsField->visible = false;

        toolbarChrome->Tick(dt, toolbarBounds, 1.0f);
        WidgetInput::ProcessInput(toolbarChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
        toolbarChrome->Draw(toolbarBounds, 1.0f);
    }

    float btnRight = bounds.x + bounds.width - 4;
    float btnY = toolbar.y + 3;

    Rectangle fullscreenBtn = {btnRight - 22, btnY, 22, 22};
    if (ToolbarIconToggle(fullscreenBtn, "icon_panel_fullscreen.png", isFullscreen)) {
        if (onToggleFullscreen) onToggleFullscreen();
    }
    btnRight -= 22 + 6;

    if (!playing) {
    Rectangle playBtn = {btnRight - 22, btnY, 22, 22};
        DrawRectangleRec(playBtn, theme.success);
        DrawText(">", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);

        if (!WidgetInput::IsMouseBlocked() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), playBtn) || IsKeyDown(KEY_LEFT_ALT) && (IsKeyDown(KEY_P)) && !playing) {
            playing = true;
            ResetFpsStats();
            savedYaw = yaw;
            savedPitch = pitch;
            savedOrbitRadius = orbitRadius;
            savedCameraTarget = cameraTarget;
            if (flying) { flying = false; ShowCursor(); }
            GameSession::BeginSession(starMap);
            }
        btnRight -= 26;
        }

        else {
        Rectangle stopBtn = {btnRight - 22, btnY, 22, 22};
            DrawRectangleRec(stopBtn, theme.dangerBg);
            DrawText("S", (int)stopBtn.x + 7, (int)stopBtn.y + 4, 13, WHITE);

        std::string requestedLevelPath;
        if (World::Get().ConsumePendingLevelLoad(requestedLevelPath)) {
            StarMap newMap;
            if (newMap.LoadFromFile(requestedLevelPath)) {
                GameSession::EndSession();
                playMap = std::move(newMap);
                GameSession::BeginSession(playMap);
            }
        }

        if (!WidgetInput::IsMouseBlocked() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), stopBtn) || IsKeyPressed(KEY_ESCAPE) && playing || World::Get().ConsumeQuitRequest()) {
            GameSession::EndSession();
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
        bool saveHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), saveBtn);
        DrawRectangleRec(saveBtn, saveHov ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(saveBtn, 1, theme.buttonBorder);
        int saveTextW = MeasureText("Save", 11);
        DrawText("Save", (int)(saveBtn.x + (saveBtn.width - saveTextW) / 2.0f),
                 (int)(saveBtn.y + (saveBtn.height - 11) / 2.0f), 11, theme.textPrimary);
        if (saveHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) SaveMap();
        btnRight -= 54;

        if (!playing) {
            Rectangle packageBtn = {btnRight - 66, btnY, 66, 22};
            bool packageHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), packageBtn);
            DrawRectangleRec(packageBtn, packageHov ? theme.buttonHover : theme.buttonBg);
            DrawRectangleLinesEx(packageBtn, 1, theme.buttonBorder);
            int packageTextW = MeasureText("Package", 11);
            DrawText("Package", (int)(packageBtn.x + (packageBtn.width - packageTextW) / 2.0f),
                     (int)(packageBtn.y + (packageBtn.height - 11) / 2.0f), 11, theme.textPrimary);
            if (packageHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) onPackageRequested();
            btnRight -= 70;
        }
    }
}

void ViewportPanel::Render() {
    const Panel* focusOwner = static_cast<const Panel*>(this);
    if (playing) WidgetInput::PushModal(focusOwner, WidgetInput::kEditorDevice);
    else WidgetInput::PopModal(focusOwner, WidgetInput::kEditorDevice);

    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    Rectangle client  = {bounds.x, toolbar.y + toolbarHeight, bounds.width, bounds.height - toolbarHeight};
    int clientWidth   = (int)client.width;
    int clientHeight  = (int)client.height;

    if (clientWidth > 0 && clientHeight > 0) {
        {
            PROFILE_SCOPE("Ensure Render Target");
            EnsureRenderTarget(clientWidth, clientHeight);
        }
        {
            PROFILE_SCOPE("Update Camera");
            UpdateCamera(client);
        }
        {
            PROFILE_SCOPE("Handle Interaction");
            HandleMapInteraction(client);
        }

        Color clearColor = {28, 28, 30, 255};

        if (playing) {
            float scale = RenderSettings::Get().GetRenderScale();
            int pieWidth = (int)(clientWidth * scale);
            int pieHeight = (int)(clientHeight * scale);
            if (pieWidth < 1) pieWidth = 1;
            if (pieHeight < 1) pieHeight = 1;
            {
                PROFILE_SCOPE("Ensure PIE Render Target");
                EnsurePieRenderTarget(pieWidth, pieHeight);
            }

            Vector2 panelMouse = {GetMousePosition().x - client.x, GetMousePosition().y - client.y};
            Vector2 lowResMouse = {panelMouse.x * ((float)pieWidth / (float)clientWidth), panelMouse.y * ((float)pieHeight / (float)clientHeight)};
            {
                PROFILE_SCOPE("Widget Tick (Screen Space)");
                WidgetRenderer::TickScreenSpaceWidgets(GetFrameTime(), {0, 0, (float)pieRenderTarget.texture.width, (float)pieRenderTarget.texture.height}, lowResMouse, false, scale);
                WidgetRenderer::TickScreenSpaceWidgets(GetFrameTime(), {0, 0, (float)clientWidth, (float)clientHeight}, panelMouse, true);
            }

            RenderPipeline::Render(camera, pieRenderTarget, clearColor);

            {
                PROFILE_RENDER_SCOPE("PIE Composite");
                BeginTextureMode(renderTarget);
                Rectangle pieSource = {0, 0, (float)pieRenderTarget.texture.width, -(float)pieRenderTarget.texture.height};
                Rectangle pieDest = {0, 0, (float)clientWidth, (float)clientHeight};
                DrawTexturePro(pieRenderTarget.texture, pieSource, pieDest, {0, 0}, 0.0f, WHITE);
                WidgetRenderer::DrawScreenSpaceWidgets({0, 0, (float)clientWidth, (float)clientHeight}, true);
                BeginMode3D(camera);
                DrawAndClearCollisionDebug((float)renderTargetHeight);
                EndMode3D();
                PrintMessageLog::Get().Draw();
                EndTextureMode();
            }
        } else {
            PROFILE_RENDER_SCOPE("Editor Scene");
            BeginTextureMode(renderTarget);
            ClearBackground(clearColor);
            RenderScene();
            EndTextureMode();
        }

        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        PROFILE_RENDER_SCOPE("Blit Viewport");
        DrawTextureRec(renderTarget.texture, source, {client.x, client.y}, WHITE);
    }

    const UIPalette& theme = UITheme::Get();
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    if (playing) {
        DrawRectangleLinesEx(client, 3, theme.success);
    }
    {
        PROFILE_SCOPE("Toolbar");
        RenderToolbar();
    }

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
