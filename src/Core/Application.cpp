#include "Application.h"
#include "Editor/Editor.h"
#include "Core/Project.h"
#include "raylib.h"
#include "raygui.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static void GuiLoadStyleDark() {
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x525252ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x252525ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xaaaaaaff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x909090ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x353535ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x0492c7ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x353535ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_PADDING, 0);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x121212ff);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
}

static bool CreateProject(const std::string& name) {
    char* userProfile = getenv("USERPROFILE");
    fs::path docsPath = userProfile ? userProfile : "";
    docsPath /= "Documents";
    docsPath /= "StarProjects";
    fs::create_directories(docsPath);

    fs::path projPath = docsPath / name;
    fs::create_directories(projPath);
    fs::create_directories(projPath / "Content");
    fs::create_directories(projPath / "Config");
    fs::create_directories(projPath / "Saved");

    fs::path projFile = projPath / (name + ".starproject");
    std::ofstream out(projFile);
    out << "name=" << name << "\n";
    out << "version=0.1.0\n";
    out.close();

    Project::Get().Load(projFile.string());
    return true;
}

bool Application::Initialize(int argc, char* argv[]) {
    int screenWidth = 1280;
    int screenHeight = 720;

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Star Engine");
    SetTargetFPS(60);

    GuiLoadStyleDark();

    if (!ModuleManager::Get().InitializeAll()) {
        return false;
    }

    editor = new Editor();
    editor->Initialize();

    if (argc > 1) {
        Project::Get().Load(argv[1]);
    } else {
        editor->ShowProjectBrowser();
    }

    running = true;
    return true;
}

void Application::Run() {
    while (running && !WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        ModuleManager::Get().UpdateAll(deltaTime);

        if (editor->WantsNewProject()) {
            std::string name = editor->GetNewProjectName();
            if (!name.empty()) CreateProject(name);
            editor->ResetNewProjectFlag();
        }

        BeginDrawing();
        ClearBackground({25, 25, 25, 255});
        if (editor) editor->Render();
        EndDrawing();
    }
}

void Application::Shutdown() {
    delete editor;
    CloseWindow();
    ModuleManager::Get().ShutdownAll();
}