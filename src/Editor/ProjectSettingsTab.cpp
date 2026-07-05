#include "ProjectSettingsTab.h"
#include "PropertyFields.h"
#include "MenuBar.h"
#include "Core/Project.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

void ProjectSettingsTab::Initialize() {
    title     = "Project Settings";
    permanent = false;
    RefreshFromProject();
}

void ProjectSettingsTab::RefreshFromProject() {
    const ProjectSettings& settings = Project::Get().GetSettings();
    snprintf(widthBuf, sizeof(widthBuf), "%d", settings.targetWidth);
    snprintf(heightBuf, sizeof(heightBuf), "%d", settings.targetHeight);
    activeField = 0;
    mapPicker.Close();
}

static std::string MapDisplayName(const std::string& path) {
    if (path.empty()) return "None";
    return fs::path(path).stem().string();
}

void ProjectSettingsTab::Render(float x, float y, float width, float height) {
    DrawRectangleRec({x, y, width, height}, {32, 32, 35, 255});

    ProjectSettings& settings = Project::Get().GetSettings();
    Vector2 mp = GetMousePosition();

    float fx = x + 16;
    float fy = y + 16;
    float fw = width - 32 > 420.0f ? 420.0f : width - 32;

    DrawText("General", (int)fx, (int)fy, 14, {180, 200, 230, 255});
    fy += 24;

    DrawText("Project Name", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle nameRow = {fx, fy, fw, 20};
    DrawRectangleRec(nameRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(nameRow, 1, {55, 55, 60, 255});
    DrawText(settings.name.c_str(), (int)nameRow.x + 6, (int)nameRow.y + 4, 12, {150, 150, 158, 255});
    fy += 34;

    DrawText("Maps", (int)fx, (int)fy, 14, {180, 200, 230, 255});
    fy += 24;

    DrawText("Default Map", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle mapRow = {fx, fy, fw, 20};
    DrawRectangleRec(mapRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(mapRow, 1, {55, 55, 60, 255});
    DrawText(MapDisplayName(settings.defaultMapPath).c_str(), (int)mapRow.x + 6, (int)mapRow.y + 4, 12, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, mapRow) && !mapPicker.IsOpen()) {
        mapPicker.Open({mapRow.x, mapRow.y + mapRow.height}, ".starmap");
    }
    fy += 20;
    DrawText("Map loaded on project open and used as the packaged game's startup map.",
              (int)fx, (int)fy, 10, {110, 110, 118, 255});
    fy += 24;
    std::string pickedMap;
    if (mapPicker.Render(pickedMap)) settings.defaultMapPath = pickedMap;

    DrawText("Rendering", (int)fx, (int)fy, 14, {180, 200, 230, 255});
    fy += 24;

    DrawText("Target Resolution", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    float colW = fw / 2.0f;
    float boxW = colW - 18.0f;
    Rectangle widthRow  = {fx + 16, fy, boxW, 20};
    Rectangle heightRow = {fx + colW + 16, fy, boxW, 20};
    DrawText("W", (int)fx, (int)widthRow.y + 4, 11, {150, 150, 160, 255});
    DrawText("H", (int)(fx + colW), (int)heightRow.y + 4, 11, {150, 150, 160, 255});
    PropertyField::IntBox(widthRow, widthBuf, sizeof(widthBuf), 1, activeField, settings.targetWidth);
    PropertyField::IntBox(heightRow, heightBuf, sizeof(heightBuf), 2, activeField, settings.targetHeight);
    fy += 20;
    DrawText("Resolution of the window opened by a packaged game. Does not affect the editor viewport.",
              (int)fx, (int)fy, 10, {110, 110, 118, 255});
    fy += 24;
}

void ProjectSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
