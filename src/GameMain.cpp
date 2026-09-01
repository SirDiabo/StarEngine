#include "Core/Module.h"
#include "Core/ContentPathResolver.h"
#include "Core/SavePaths.h"
#include "Core/CookedFile.h"
#include "Core/PackageCipher.h"
#include "Core/StarPak.h"
#include "Core/ExecutablePath.h"
#include "Core/InputMappingRegistry.h"
#include "Runtime/PackagedGameConfig.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/StarMap.h"
#include "Runtime/AssetCache.h"
#include "Runtime/AudioCore.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Core/Widgets/WidgetRenderer.h"
#include "Core/Widgets/WidgetDebug.h"
#include "Core/RenderPipeline.h"
#include "Core/GameSession.h"
#include "Core/RenderSettings.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "Modules.modules.generated.h"
#include "raylib.h"
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
    std::string resolvedExe = GetExecutablePath();
    fs::path exePath = !resolvedExe.empty() ? fs::path(resolvedExe) : (argc > 0 ? fs::path(argv[0]) : fs::path());
    std::error_code ec;
    if (!exePath.empty()) exePath = fs::absolute(exePath, ec);
    fs::path exeDir = exePath.empty() ? fs::current_path() : exePath.parent_path();
    if (exeDir.empty()) exeDir = fs::current_path();
    exeDir = fs::absolute(exeDir, ec);

    PackagedGameConfig config;

    StarPak::Reader pak;
    if (!exePath.empty() && pak.LoadFromExecutable(exePath.string())) {
        std::vector<uint8_t> bootBytes = pak.ReadEntry(StarPak::kBootConfigPath);
        if (bootBytes.empty()) {
            TraceLog(LOG_ERROR, "GameMain: pak loaded but boot config entry was empty");
            return 1;
        }
        if (!PackagedGameConfig::LoadFromText(std::string(bootBytes.begin(), bootBytes.end()), config)) {
            TraceLog(LOG_ERROR, "GameMain: failed to parse boot config from pak");
            return 1;
        }
        if (pak.HasEntry(StarPak::kInputMappingsPath)) {
            std::vector<uint8_t> mappingBytes = pak.ReadEntry(StarPak::kInputMappingsPath);
            InputMappingRegistry::Get().LoadFromText(std::string(mappingBytes.begin(), mappingBytes.end()));
        }
        CookedFile::SetActivePak(std::move(pak), CookCipherFromName(config.cipher), config.cipherKey);
        TraceLog(LOG_INFO, "GameMain: loaded pak, name=%s defaultMap=%s", config.name.c_str(), config.defaultMap.c_str());
    } else {
        std::string bootstrapPath = FindBootstrapFile(exeDir);
        if (bootstrapPath.empty() || !PackagedGameConfig::LoadFromFile(bootstrapPath, config)) {
            TraceLog(LOG_ERROR, "GameMain: no embedded pak and no .starproject bootstrap found next to the exe");
            return 1;
        }
        ContentPathResolver::SetContentRoot((exeDir / "Content").string());
        CookedFile::SetActiveCipher(CookCipherFromName(config.cipher), config.cipherKey);
        InputMappingRegistry::Get().Load(exeDir.string());
        TraceLog(LOG_INFO, "GameMain: loaded loose bootstrap, name=%s defaultMap=%s", config.name.c_str(), config.defaultMap.c_str());
    }

    RegisterAllModules();
    SavePaths::SetPackagedGameName(config.name);
    if (!ModuleManager::Get().InitializeAll()) return 1;

    RenderSettings::Get().SetRenderScale(config.renderScale);
    RenderSettings::Get().SetShadingType(config.shadingType);
    RenderSettings::Get().SetLightingQuality(LightingQualityFromName(config.lightingQuality));

    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(config.shadingType);
    if (activeTechnique && activeTechnique->applyPackagedSettings) {
        activeTechnique->applyPackagedSettings(config.moduleSettings);
    }

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
    if (!config.defaultMap.empty()) {
        if (!map.LoadFromFile(config.defaultMap)) {
            TraceLog(LOG_ERROR, "GameMain: failed to load default map '%s'", config.defaultMap.c_str());
        }
    } else {
        TraceLog(LOG_WARNING, "GameMain: no default map set, starting with an empty world");
    }
    GameSession::BeginSession(map);
    TraceLog(LOG_INFO, "GameMain: spawned %d actors", (int)World::Get().GetActors().size());

    Camera3D camera{};
    camera.position = {0, 400, 800};
    camera.target = {0, 0, 0};
    camera.up = {0, 1, 0};
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    GameSession::UpdateActiveCamera(camera);

    double perfTickAccum = 0.0, perfRenderAccum = 0.0, perfPresentAccum = 0.0;
    int perfSampleCount = 0;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        double perfTickStart = GetTime();
        ModuleManager::Get().UpdateAll(deltaTime);
        World::Get().Tick(deltaTime);
        perfTickAccum += GetTime() - perfTickStart;

        GameSession::UpdateActiveCamera(camera);

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

        WidgetRenderer::TickScreenSpaceWidgets(deltaTime, {0, 0, (float)scaledWidth, (float)scaledHeight});

        double perfRenderStart = GetTime();
        RenderPipeline::Render(camera, renderTarget, {20, 20, 20, 255});
        perfRenderAccum += GetTime() - perfRenderStart;

        double perfPresentStart = GetTime();
        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle source = {0, 0, (float)renderTarget.texture.width, -(float)renderTarget.texture.height};
        Rectangle dest = {0, 0, (float)screenWidth, (float)screenHeight};
        DrawTexturePro(renderTarget.texture, source, dest, {0, 0}, 0.0f, WHITE);
        WidgetRenderer::DrawScreenSpaceWidgets({0, 0, (float)screenWidth, (float)screenHeight}, true);
        EndDrawing();
        perfPresentAccum += GetTime() - perfPresentStart;

        if (++perfSampleCount >= 120) {
            TraceLog(LOG_INFO, "PERF tick=%.3fms render=%.3fms present=%.3fms (avg over %d frames)",
                (perfTickAccum / perfSampleCount) * 1000.0,
                (perfRenderAccum / perfSampleCount) * 1000.0,
                (perfPresentAccum / perfSampleCount) * 1000.0,
                perfSampleCount);
            perfTickAccum = perfRenderAccum = perfPresentAccum = 0.0;
            perfSampleCount = 0;
        }
    }

    if (renderTarget.id != 0) UnloadRenderTexture(renderTarget);
    GameSession::EndSession();
    AssetCache::Get().Clear();
    AudioCore::Get().CloseDevice();
    ModuleManager::Get().ShutdownAll();
    CloseWindow();
    return 0;
}
