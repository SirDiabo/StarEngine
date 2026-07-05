#include "ViewportPanel.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/Components/AudioComponent.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "raygui.h"
#include "raymath.h"
#include "rlgl.h"
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
    UnloadMeshCache();
}

void ViewportPanel::UnloadMeshCache() {
    for (auto& kv : meshCache) UnloadModel(kv.second);
    meshCache.clear();
    for (auto& kv : textureCache) UnloadTexture(kv.second);
    textureCache.clear();
}

Model* ViewportPanel::GetOrLoadModel(const std::string& sourceMeshPath) {
    auto it = meshCache.find(sourceMeshPath);
    if (it != meshCache.end()) return &it->second;

    if (sourceMeshPath.empty() || !FileExists(sourceMeshPath.c_str())) return nullptr;

    Model model = LoadModel(sourceMeshPath.c_str());
    if (model.meshCount == 0) {
        UnloadModel(model);
        return nullptr;
    }

    auto inserted = meshCache.emplace(sourceMeshPath, model);
    return &inserted.first->second;
}

Texture2D* ViewportPanel::GetOrLoadTexture(const std::string& textureAssetPath) {
    auto it = textureCache.find(textureAssetPath);
    if (it != textureCache.end()) return &it->second;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(textureAssetPath, asset) || asset.assetType != "Texture") return nullptr;
    if (asset.sourcePath.empty() || !FileExists(asset.sourcePath.c_str())) return nullptr;

    Texture2D tex = LoadTexture(asset.sourcePath.c_str());
    if (tex.id == 0) return nullptr;

    auto inserted = textureCache.emplace(textureAssetPath, tex);
    return &inserted.first->second;
}

const Stargraph* ViewportPanel::GetOrLoadClassGraph(const std::string& graphAssetPath) {
    auto it = classGraphCache.find(graphAssetPath);
    if (it != classGraphCache.end()) return &it->second;

    if (graphAssetPath.empty() || !fs::exists(graphAssetPath)) return nullptr;

    Stargraph graph;
    if (!graph.LoadFromFile(graphAssetPath)) return nullptr;

    auto inserted = classGraphCache.emplace(graphAssetPath, std::move(graph));
    return &inserted.first->second;
}

void ViewportPanel::InvalidateClassGraph(const std::string& graphAssetPath) {
    classGraphCache.erase(graphAssetPath);
}

void ViewportPanel::DrawInstanceOrMesh(const std::string& meshAssetPath, const std::string& materialAssetPath,
                                        Vector3 pos, Vector3 rot, Vector3 scale, bool selected, Color fallbackColor) {
    Model* model = nullptr;
    if (!meshAssetPath.empty()) {
        StarAsset asset;
        if (StarAsset::LoadFromFile(meshAssetPath, asset) && asset.assetType == "StaticMesh") {
            model = GetOrLoadModel(asset.sourcePath);
        }
    }

    if (model) {
        Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
        Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
        model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);

        Color tint = WHITE;
        if (!materialAssetPath.empty()) {
            StarMaterial material;
            if (StarMaterial::LoadFromFile(materialAssetPath, material)) {
                tint = material.baseColor;
                if (!material.textureAssetPath.empty()) {
                    if (Texture2D* tex = GetOrLoadTexture(material.textureAssetPath)) {
                        for (int i = 0; i < model->materialCount; i++) {
                            model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
                        }
                    }
                }
            }
        }

        DrawModel(*model, pos, 1.0f, tint);
        if (selected) DrawModelWires(*model, pos, 1.0f, {255, 200, 50, 255});
    } else {
        DrawCube(pos, scale.x, scale.y, scale.z, fallbackColor);
        DrawCubeWires(pos, scale.x, scale.y, scale.z, {0, 0, 0, 120});
        if (!meshAssetPath.empty()) {
            // A mesh is referenced but couldn't be resolved.
            DrawCubeWires(pos, scale.x * 1.08f, scale.y * 1.08f, scale.z * 1.08f, {220, 40, 200, 200});
        }
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
    classGraphCache.clear();
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
    fs::path src = asset.sourcePath.empty() ? fs::path(assetPath) : fs::path(asset.sourcePath);
    std::string baseName = src.stem().string();

    ActorInstance inst;
    inst.name = baseName + "_" + std::to_string(starMap.GetInstances().size() + 1);
    inst.className = "StaticMeshActor";
    inst.meshAssetPath = assetPath;
    inst.posX = worldX + asset.importPosX;
    inst.posY = asset.importPosY;
    inst.posZ = worldZ + asset.importPosZ;
    inst.rotX = asset.importRotX;
    inst.rotY = asset.importRotY;
    inst.rotZ = asset.importRotZ;
    // Mesh scale is a 1.0 = original-size multiplier, unlike the placeholder
    // box scale (which is an absolute box dimension) used by graph actors.
    // Starts from the asset's stored import scale rather than always 1.0.
    inst.scaleX = asset.importScaleX;
    inst.scaleY = asset.importScaleY;
    inst.scaleZ = asset.importScaleZ;
    if (!asset.defaultMaterialPath.empty()) inst.materialAssetPath = asset.defaultMaterialPath;

    starMap.AddInstance(inst);
    selectedActorIndex = (int)starMap.GetInstances().size() - 1;
    mapDirty = true;
}

void ViewportPanel::SpawnActorFromInstance(const ActorInstance& inst) {
    Actor* actor = World::Get().SpawnActor(inst.className, inst.name);
    actor->position = {inst.posX, inst.posY, inst.posZ};
    actor->rotation = {inst.rotX, inst.rotY, inst.rotZ};
    actor->scale    = {inst.scaleX, inst.scaleY, inst.scaleZ};

    if (!inst.graphAssetPath.empty() && fs::exists(inst.graphAssetPath)) {
        auto* behavior = new StargraphBehavior(Stargraph{});
        behavior->GetGraph().LoadFromFile(inst.graphAssetPath);
        actor->SetGraphBehavior(behavior);

        for (const ComponentDecl& decl : behavior->GetGraph().GetComponents()) {
            Component* comp = nullptr;
            if (decl.componentType == "MeshComponent") {
                auto* mesh = new MeshComponent();
                mesh->SetMeshPath(decl.meshAssetPath);
                mesh->SetMaterialPath(decl.materialAssetPath);
                comp = mesh;
            } else if (decl.componentType == "CollisionComponent") {
                auto* collision = new CollisionComponent();
                collision->shapeType = decl.collisionShape == "Sphere" ? CollisionComponent::Sphere
                                     : decl.collisionShape == "Capsule" ? CollisionComponent::Capsule
                                     : CollisionComponent::Box;
                collision->radius = decl.collisionRadius;
                collision->height = decl.collisionHeight;
                comp = collision;
            } else if (decl.componentType == "AudioComponent") {
                auto* audio = new AudioComponent();
                audio->soundPath = decl.soundAssetPath;
                audio->autoPlay = decl.autoPlay;
                audio->looping = decl.looping;
                audio->volume = decl.volume;
                comp = audio;
            } else {
                comp = new SceneComponent();
            }

            comp->position = {decl.posX, decl.posY, decl.posZ};
            comp->rotation = {decl.rotX, decl.rotY, decl.rotZ};
            comp->scale    = {decl.scaleX, decl.scaleY, decl.scaleZ};
            actor->AddComponent(comp);
        }
    }

    if (!inst.meshAssetPath.empty()) {
        auto* meshComp = new MeshComponent();
        meshComp->SetMeshPath(inst.meshAssetPath);
        meshComp->SetMaterialPath(inst.materialAssetPath);
        actor->AddComponent(meshComp);
    }
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
    return {200, 160, 80, 255};
}

void ViewportPanel::RenderScene() {
    const float placeholderSize = 50.0f;

    BeginMode3D(camera);
    DrawGrid(40, 100.0f);

    if (playing) {
        for (const auto& actor : World::Get().GetActors()) {
            Vector3 basePos = actor->position;
            Vector3 baseRot = actor->rotation;
            Vector3 baseScale = actor->scale;

            bool drewAnyMesh = false;
            for (Component* comp : actor->GetComponents()) {
                if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                    Vector3 pos = Vector3Add(basePos, comp->position);
                    Vector3 rot = Vector3Add(baseRot, comp->rotation);
                    Vector3 scl = Vector3Multiply(baseScale, comp->scale);
                    DrawInstanceOrMesh(mc->meshPath, mc->materialPath, pos, rot, scl, false, WHITE);
                    drewAnyMesh = true;
                }
            }
            if (!drewAnyMesh) {
                Vector3 boxSize = {placeholderSize * baseScale.x, placeholderSize * baseScale.y, placeholderSize * baseScale.z};
                DrawInstanceOrMesh("", "", basePos, baseRot, boxSize, false, {100, 200, 100, 255});
            }
        }
    } else {
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

                if (classGraph) {
                    for (const ComponentDecl& comp : classGraph->GetComponents()) {
                        Vector3 compPos = {pos.x + comp.posX, pos.y + comp.posY, pos.z + comp.posZ};
                        Vector3 compRot = {rot.x + comp.rotX, rot.y + comp.rotY, rot.z + comp.rotZ};
                        Vector3 compScale = {scl.x * comp.scaleX, scl.y * comp.scaleY, scl.z * comp.scaleZ};

                        if (comp.componentType == "MeshComponent") {
                            DrawInstanceOrMesh(comp.meshAssetPath, comp.materialAssetPath, compPos, compRot,
                                                compScale, selected, fallbackColor);
                            drewAnyMesh = true;
                        } else if (comp.componentType == "CollisionComponent" && selected) {
                            DrawCollisionShape(comp, compPos, compRot, {90, 200, 255, 200});
                        }
                    }
                }

                if (!drewAnyMesh) {
                    Vector3 boxSize = {placeholderSize * scl.x, placeholderSize * scl.y, placeholderSize * scl.z};
                    DrawInstanceOrMesh("", "", pos, rot, boxSize, false, fallbackColor);
                }
            } else {
                DrawInstanceOrMesh(inst.meshAssetPath, inst.materialAssetPath, pos, rot, scl, selected, fallbackColor);
            }
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
                 SpawnActorFromInstance(inst);
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