#include "Application.h"
#include "Editor/Editor.h"
#include "raylib.h"

bool Application::Initialize() {
    int screenWidth = 1280;
    int screenHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "StarEngine");
    SetTargetFPS(60);

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