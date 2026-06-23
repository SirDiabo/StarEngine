#include "Application.h"
#include "Editor/Editor.h"
#include "Editor/LogPanel.h"
#include "Core/Project.h"
#include "Core/Module.h"
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
    if (activeLogPanel) activeLogPanel->AddLog(buffer);
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
    InitWindow(1280, 720, "Star Engine");
    SetTargetFPS(60);

    editor = new Editor();
    editor->Initialize();

    ModuleManager::Get().InitializeAll();

    activeLogPanel = editor->GetLogPanel();
    SetTraceLogCallback(ForwardTraceLogToPanel);
    SetTraceLogLevel(LOG_INFO);

    if (argc > 1) {
        Project::Get().Load(argv[1]);
    } else {
        editor->ShowProjectBrowser();
    }

    running = true;
    return true;
}

void Application::Run() {
    while (!WindowShouldClose() && running) {
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
                editor->ResetNewProjectFlag();
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
        editor->Render();
        EndDrawing();
    }
}

void Application::Shutdown() {
    delete editor;
    editor = nullptr;
    CloseWindow();
}