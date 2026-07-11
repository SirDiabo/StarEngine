#include "Core/Module.h"
#include "Core/ContentPathResolver.h"
#include "Core/CookedFile.h"
#include "Core/PackageCipher.h"
#include "Runtime/PackagedGameConfig.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/StarMap.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/SceneRenderer.h"
#include "Runtime/AssetCache.h"
#include "Runtime/AudioCore.h"
#include "Runtime/Components/CameraComponent.h"
#include "Modules.modules.generated.h"
#include "raylib.h"
#include "raymath.h"
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static std::string FindBootstrapFile(const fs::path& exeDir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(exeDir, ec)) {
        if (entry.path().extension() == ".starproject") return entry.path().string();
    }
    return "";
}

int main(int argc, char* argv[]) {
    fs::path exeDir = argc > 0 ? fs::path(argv[0]).parent_path() : fs::path();
    if (exeDir.empty()) exeDir = fs::current_path();
    std::error_code ec;
    exeDir = fs::absolute(exeDir, ec);

    std::string bootstrapPath = FindBootstrapFile(exeDir);
    PackagedGameConfig config;
    if (bootstrapPath.empty() || !PackagedGameConfig::LoadFromFile(bootstrapPath, config)) {
        return 1;
    }

    ContentPathResolver::SetContentRoot((exeDir / "Content").string());
    CookedFile::SetActiveCipher(CookCipherFromName(config.cipher), config.cipherKey);

    RegisterAllModules();
    if (!ModuleManager::Get().InitializeAll()) return 1;

    InitWindow(config.targetWidth, config.targetHeight, config.name.c_str());
    SetTargetFPS(60);
    AudioCore::Get().InitDevice();

    fs::path iconPath = exeDir / "icon.png";
    if (fs::exists(iconPath)) {
        Image icon = LoadImage(iconPath.string().c_str());
        if (icon.data) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }

    StarMap map;
    if (!config.defaultMap.empty()) map.LoadFromFile(config.defaultMap);
    WorldSpawner::SpawnMapActors(map);
    World::Get().BeginPlay();

    Camera3D camera{};
    camera.position = {0, 400, 800};
    camera.target = {0, 0, 0};
    camera.up = {0, 1, 0};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    for (const auto& actor : World::Get().GetActors()) {
        if (actor->GetCameraComponent()) {
            CameraComponent* cc = actor->GetCameraComponent();
            Vector3 compPos = Actor::ComputeWorldPosition(cc->position, actor->position, actor->rotation, actor->scale);
            Vector3 compRot = Actor::ComputeWorldRotation(cc->rotation, actor->rotation);
            float yr = compRot.y * DEG2RAD;
            float pr = compRot.x * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            camera.position = compPos;
            camera.target = Vector3Add(compPos, forward);
            camera.fovy = cc->fov;
            camera.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
            break;
        }
    }

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        ModuleManager::Get().UpdateAll(deltaTime);
        World::Get().Tick(deltaTime);

        for (const auto& actor : World::Get().GetActors()) {
            if (actor->GetCameraComponent()) {
                CameraComponent* cc = actor->GetCameraComponent();
                Vector3 compPos = Actor::ComputeWorldPosition(cc->position, actor->position, actor->rotation, actor->scale);
                Vector3 compRot = Actor::ComputeWorldRotation(cc->rotation, actor->rotation);
                float yr = compRot.y * DEG2RAD;
                float pr = compRot.x * DEG2RAD;
                Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
                camera.position = compPos;
                camera.target = Vector3Add(compPos, forward);
                camera.fovy = cc->fov;
                break;
            }
        }

        BeginDrawing();
        ClearBackground({20, 20, 20, 255});
        BeginMode3D(camera);
        SceneRenderer::DrawWorldActors(false);
        EndMode3D();
        EndDrawing();
    }

    World::Get().EndPlay();
    AssetCache::Get().Clear();
    AudioCore::Get().CloseDevice();
    ModuleManager::Get().ShutdownAll();
    CloseWindow();
    return 0;
}