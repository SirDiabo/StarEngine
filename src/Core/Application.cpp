#include "Application.h"
#include "Editor/Editor.h"
#include "Editor/LogPanel.h"
#include "Core/Project.h"
#include "Core/Module.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/BuildRunner.h"
#include "Core/EmbeddedAssets.h"
#include "Core/SafeProcess.h"
#include "Runtime/StarMap.h"
#include "Runtime/World.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include "raygui.h"
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
    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "StarEngine");
    SetExitKey(0);
    SetTargetFPS(60);
    AudioCore::Get().InitDevice();

    EmbeddedAssets::Get().Init(argc > 0 ? argv[0] : "");

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

    activeLogPanel = editor->GetLogPanel();
    SetTraceLogCallback(ForwardTraceLogToPanel);
    SetTraceLogLevel(LOG_INFO);

    if (argc > 1) {
        Project::Get().Load(argv[1]);
        projectPath = argv[1];
    }

    ModuleManager::Get().InitializeAll();

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
            running = false;
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