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
#include "Runtime/Components/CameraComponent.h"
#include "Modules.modules.generated.h"
#include "raylib.h"
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
            Vector3 compPos = Vector3Add(actor->position, cc->position);
            Vector3 compRot = Vector3Add(actor->rotation, cc->rotation);
            float yr = (actor->rotation.y + cc->rotation.y) * DEG2RAD;
            float pr = (actor->rotation.x + cc->rotation.x) * DEG2RAD;
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
        World::Get().Tick(deltaTime);

        for (const auto& actor : World::Get().GetActors()) {
            if (actor->GetCameraComponent()) {
                CameraComponent* cc = actor->GetCameraComponent();
                Vector3 compPos = Vector3Add(actor->position, cc->position);
                Vector3 compRot = Vector3Add(actor->rotation, cc->rotation);
                float yr = (actor->rotation.y + cc->rotation.y) * DEG2RAD;
                float pr = (actor->rotation.x + cc->rotation.x) * DEG2RAD;
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
    ModuleManager::Get().ShutdownAll();
    CloseWindow();
    return 0;
}
