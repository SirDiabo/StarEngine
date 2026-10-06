#include "Application.h"
#include "Editor/Editor.h"
#include "Editor/LogPanel.h"
#include "Core/Project.h"
#include "Core/Module.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/StarMaterial.h"
#include "Core/BuildRunner.h"
#include "Core/EmbeddedAssets.h"
#include "Core/ExecutablePath.h"
#include "Core/SafeProcess.h"
#include "Core/Widgets/WidgetAsset.h"
#include "Core/EditorAssetHandlerRegistry.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Editor/WidgetEditorTab.h"
#include "Runtime/StarMap.h"
#include "Runtime/World.h"
#include "Runtime/AudioCore.h"
#include "Runtime/AssetCache.h"
#include "Runtime/DebugDraw.h"
#include "Runtime/Components/CameraComponent.h"
#include "Modules/Stargraph/Nodes_Collision.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <cstdarg>
#include <cstdio>

namespace fs = std::filesystem;

static LogPanel* activeLogPanel = nullptr;

static void ForwardTraceLogToPanel(int logLevel, const char* text, va_list args) {
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), text, args);
    if (!activeLogPanel) return;

    LogLevel level = LogLevel::Info;
    if (logLevel == LOG_WARNING) level = LogLevel::Warning;
    else if (logLevel == LOG_ERROR || logLevel == LOG_FATAL) level = LogLevel::Error;

    activeLogPanel->AddLog(buffer, level);
}

static bool CreateProject(const std::string& name) {
    fs::path projPath = fs::path(Project::GetProjectsDirectory()) / name;
    fs::create_directories(projPath);
    fs::create_directories(projPath / "Content");
    fs::create_directories(projPath / "Config");
    fs::create_directories(projPath / "Saved");

    fs::path projFile = projPath / (name + ".starproject");
    std::ofstream(projFile).close();

    Project::Get().Load(projFile.string());
    Project::Get().Save();
    return true;
}

bool Application::Initialize(int argc, char* argv[]) {
    SetTraceLogLevel(LOG_ALL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "StarEngine " STARENGINE_VERSION);
    SetExitKey(0);
    SetTargetFPS(60);
    AudioCore::Get().InitDevice();

    std::string exePath = GetExecutablePath();
    if (exePath.empty() && argc > 0) exePath = argv[0];
    EmbeddedAssets::Get().Init(exePath);

    {
        Image icon = EmbeddedAssets::Get().LoadImageAsset("logo.png");
        if (icon.data) {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    }

    editor = new Editor();
    editor->Initialize();

    AssetTypeInfo mapType;
    mapType.typeName = "StarMap";
    mapType.extension = ".starmap";
    mapType.category = "Scene";
    mapType.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        StarMap map;
        map.mapName = assetName;
        return map.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(mapType);

    AssetTypeInfo meshType;
    meshType.typeName = "StaticMesh";
    meshType.extension = ".starasset";
    meshType.category = "Mesh";
    AssetTypeRegistry::Get().RegisterType(meshType);

    AssetTypeInfo textureType;
    textureType.typeName = "Texture";
    textureType.extension = ".startex";
    textureType.category = "Mesh";
    AssetTypeRegistry::Get().RegisterType(textureType);

    AssetTypeInfo materialType;
    materialType.typeName = "Material";
    materialType.extension = ".starmat";
    materialType.category = "Mesh";
    materialType.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        StarMaterial mat;
        return mat.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(materialType);

    AssetTypeInfo animationType;
    animationType.typeName = "Animation";
    animationType.extension = ".staranim";
    animationType.category = "Mesh";
    AssetTypeRegistry::Get().RegisterType(animationType);

    AssetTypeInfo widgetType;
    widgetType.typeName = "Widget";
    widgetType.extension = ".starwidget";
    widgetType.category = "UI";
    widgetType.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        WidgetAssetData data;
        data.assetName = assetName;
        data.parentClass = "Widget";
        const std::vector<ProgrammingInterfaceInfo>& providers = ProgrammingInterfaceRegistry::Get().GetAll();
        if (providers.size() == 1) {
            data.programmingInterface = providers[0].name;
            if (providers[0].createDefaultContent) data.behaviorContent = providers[0].createDefaultContent();
        }
        bool saved = WidgetAsset::SaveWidgetAsset(fullPath, data);
        delete data.rootElement;
        delete data.behaviorContent;
        return saved;
    };
    widgetType.renameAsset = [](const std::string& fullPath, const std::string& newAssetName) {
        WidgetAssetData data;
        if (!WidgetAsset::LoadWidgetAsset(fullPath, data)) return false;
        data.assetName = newAssetName;
        bool saved = WidgetAsset::SaveWidgetAsset(fullPath, data);
        delete data.rootElement;
        delete data.behaviorContent;
        return saved;
    };
    AssetTypeRegistry::Get().RegisterType(widgetType);

    EditorAssetHandlerInfo widgetHandler;
    widgetHandler.extension = ".starwidget";
    widgetHandler.createTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        auto* tab = new WidgetEditorTab(fullPath);
        tab->SetSharedLogPanel(services.sharedLogPanel);
        tab->SetViewportPanel(services.sharedViewportPanel);
        return tab;
    };
    EditorAssetHandlerRegistry::Get().RegisterHandler(widgetHandler);

    activeLogPanel = editor->GetLogPanel();
    SetTraceLogCallback(ForwardTraceLogToPanel);
    SetTraceLogLevel(LOG_INFO);

    if (argc > 1) {
        Project::Get().Load(argv[1]);
        projectPath = argv[1];
    }

    ModuleManager::Get().InitializeAll();

    SetCollisionDebugEnabled(true);
    DebugDraw::LoadSettings();

    editor->RestoreState();

    if (argc > 1) {
        editor->OpenDefaultMapIfSet();
    } else {
        editor->ShowProjectBrowser();
    }

    running = true;
    
    BuildRunner::Get().SetRelaunchCallback([this]() {
        this->RequestRelaunch();
    });
    
    return true;
}

void Application::Run() {
    while (!WindowShouldClose() && running) {
        if (pendingRelaunch) {
            projectPath = Project::Get().GetProjectFilePath();
            editor->SaveLayout();
            editor->SaveState();
            Shutdown();
            Relaunch();
            running = false;
            break;
        }

        if (editor->WantsNewProject()) {
            std::string name = editor->GetNewProjectName();
            if (!name.empty()) {
                CreateProject(name);
                editor->ResetNewProjectFlag();
            }
        }

        if (editor->WantsOpenProject()) {
            std::string path = editor->GetOpenProjectPath();
            if (!path.empty()) {
                if (World::Get().IsPlaying()) World::Get().EndPlay();
                editor->CloseAllAssetEditorTabs();
                AssetCache::Get().Clear();
                StargraphAssetCache::Get().Clear();
                Project::Get().Load(path);
                ModuleManager::Get().ShutdownAll();
                ModuleManager::Get().InitializeAll();
                editor->RestoreState();
                editor->ResetNewProjectFlag();
                editor->OpenDefaultMapIfSet();
            }
        }

        if (editor->WantsSaveProject()) {
            Project::Get().Save();
            editor->ResetSaveProjectFlag();
        }

        if (editor->WantsCancel()) {
            if (Project::Get().GetRootPath().empty()) {
                running = false;
            }
            editor->ResetCancelFlag();
        }

        BeginDrawing();
        ClearBackground({20, 20, 20, 255});
        float deltaTime = GetFrameTime();
        ModuleManager::Get().UpdateAll(deltaTime);
        World::Get().Tick(deltaTime);
        editor->Render();
        EndDrawing();
    }
}

void Application::Shutdown() {
    delete editor;
    editor = nullptr;
    AudioCore::Get().CloseDevice();
    CloseWindow();
}

void Application::RequestRelaunch() {
    pendingRelaunch = true;
}

void Application::Relaunch() {
    fs::path exeDir = GetApplicationDirectory();
    std::string exeName = "StarEngine";
#ifdef _WIN32
    exeName = "StarEngine.exe";
#endif

    std::vector<std::string> argv = { (exeDir / exeName).string() };
    if (!projectPath.empty()) argv.push_back(projectPath);

    std::string log;
    SafeProcess::Run(argv, log);
}
