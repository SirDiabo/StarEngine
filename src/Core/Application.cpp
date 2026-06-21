#include "Application.h"
#include "Editor/Editor.h"
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

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

bool Application::Initialize() {
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

    running = true;
    return true;
}

void Application::Run() {
    while (running && !WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        ModuleManager::Get().UpdateAll(deltaTime);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (editor) editor->Render();
        EndDrawing();
    }
}

void Application::Shutdown() {
    delete editor;
    CloseWindow();
    ModuleManager::Get().ShutdownAll();
}