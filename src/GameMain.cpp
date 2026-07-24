#include "Core/Module.h"
#include "Core/ContentPathResolver.h"
#include "Core/SavePaths.h"
#include "Core/CookedFile.h"
#include "Core/PackageCipher.h"
#include "Core/StarPak.h"
#include "Runtime/PackagedGameConfig.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/StarMap.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/AssetCache.h"
#include "Runtime/AudioCore.h"
#include "Runtime/Components/CameraComponent.h"
#include "Modules/Rendering/RenderPipeline.h"
#include "Core/RenderSettings.h"
#include "Modules.modules.generated.h"
#include "raylib.h"
#include "raymath.h"
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string FindBootstrapFile(const fs::path& exeDir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(exeDir, ec)) {
        if (entry.path().extension() == ".starproject") return entry.path().string();
    }
    return "";
}

int main(int argc, char* argv[]) {
    fs::path exePath = argc > 0 ? fs::path(argv[0]) : fs::path();
    fs::path exeDir = exePath.empty() ? fs::current_path() : exePath.parent_path();
    if (exeDir.empty()) exeDir = fs::current_path();
    std::error_code ec;
    exeDir = fs::absolute(exeDir, ec);
    if (!exePath.empty()) exePath = fs::absolute(exePath, ec);

    PackagedGameConfig config;

    StarPak::Reader pak;
    if (!exePath.empty() && pak.LoadFromExecutable(exePath.string())) {
        std::vector<uint8_t> bootBytes = pak.ReadEntry(StarPak::kBootConfigPath);
        if (bootBytes.empty()) return 1;
        if (!PackagedGameConfig::LoadFromText(std::string(bootBytes.begin(), bootBytes.end()), config)) {
            return 1;
        }
        CookedFile::SetActivePak(std::move(pak), CookCipherFromName(config.cipher), config.cipherKey);
    } else {
        std::string bootstrapPath = FindBootstrapFile(exeDir);
        if (bootstrapPath.empty() || !PackagedGameConfig::LoadFromFile(bootstrapPath, config)) {
            return 1;
        }
        ContentPathResolver::SetContentRoot((exeDir / "Content").string());
        CookedFile::SetActiveCipher(CookCipherFromName(config.cipher), config.cipherKey);
    }

    RegisterAllModules();
    SavePaths::SetPackagedGameName(config.name);
    if (!ModuleManager::Get().InitializeAll()) return 1;

    RenderSettings::Get().SetRenderScale(config.renderScale);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, config.name.c_str());
    SetTargetFPS(60);
    AudioCore::Get().InitDevice();

    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

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
            Vector3 compPos = actor->GetComponentWorldPosition(cc);
            Vector3 compRot = actor->GetComponentWorldRotation(cc);
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
                Vector3 compPos = actor->GetComponentWorldPosition(cc);
                Vector3 compRot = actor->GetComponentWorldRotation(cc);
                float yr = compRot.y * DEG2RAD;
                float pr = compRot.x * DEG2RAD;
                Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
                camera.position = compPos;
                camera.target = Vector3Add(compPos, forward);
                camera.fovy = cc->fov;
                break;
            }
        }

        if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER)) {
            ToggleBorderlessWindowed();
        }

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        int scaledWidth = (int)(screenWidth * RenderSettings::Get().GetRenderScale());
        int scaledHeight = (int)(screenHeight * RenderSettings::Get().GetRenderScale());
        if (scaledWidth < 1) scaledWidth = 1;
        if (scaledHeight < 1) scaledHeight = 1;

        if (scaledWidth != renderTargetWidth || scaledHeight != renderTargetHeight) {
            if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
            renderTarget = LoadRenderTexture(scaledWidth, scaledHeight);
            SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_POINT);
            renderTargetWidth = scaledWidth;
            renderTargetHeight = scaledHeight;
        }

        RenderPipeline::Render(camera, renderTarget, {20, 20, 20, 255}, false);

        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        Rectangle dest = {0, 0, (float)screenWidth, (float)screenHeight};
        DrawTexturePro(renderTarget.texture, source, dest, {0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    World::Get().EndPlay();
    AssetCache::Get().Clear();
    AudioCore::Get().CloseDevice();
    ModuleManager::Get().ShutdownAll();
    CloseWindow();
    return 0;
}