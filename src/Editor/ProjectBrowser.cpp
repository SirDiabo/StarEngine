#include "ProjectBrowser.h"
#include "Core/EmbeddedAssets.h"
#include "Core/Project.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

void ProjectBrowser::ResetFlags() {
    createConfirmed = false;
    openProject = false;
    visible = false;
    wantsCreate = false;
}

void ProjectBrowser::Show(Mode m, bool allowCancel) {
    visible = true;
    mode = m;
    wantsCreate = (m == Mode::Create);
    createConfirmed = false;
    openProject = false;
    wantsCancel = false;
    showCancel = allowCancel;
    ScanProjects();
}

void ProjectBrowser::ScanProjects() {
    projects.clear();
    fs::path docsPath = Project::GetProjectsDirectory();

    if (fs::exists(docsPath)) {
        for (const auto& entry : fs::directory_iterator(docsPath)) {
            if (entry.is_directory()) {
                fs::path projFile = entry.path() / (entry.path().filename().string() + ".starproject");
                ProjectInfo info;
                info.name = entry.path().filename().string();
                info.path = projFile.string();
                info.exists = fs::exists(projFile);
                projects.push_back(info);
            }
        }
    }
}

bool ProjectBrowser::IsNameUnique(const std::string& name) const {
    for (const auto& proj : projects) {
        if (proj.name == name) return false;
    }
    return true;
}

void ProjectBrowser::LoadLogoIfNeeded() {
    if (logoLoadAttempted) return;
    logoLoadAttempted = true;

    logo = EmbeddedAssets::Get().LoadTextureAsset("logo.png");
    if (logo.id != 0) logoLoaded = true;
}

void ProjectBrowser::Render() {
    if (!visible) return;
    LoadLogoIfNeeded();

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    Rectangle modal = { (float)width/2 - 200, (float)height/2 - 150, 400, 300 };

    DrawRectangle(0, 0, width, height, Fade(BLACK, 0.5f));
    DrawRectangleRec(modal, {40, 40, 40, 255});
    DrawRectangleLinesEx(modal, 1, GRAY);

    float titleX = modal.x + 10;
    if (logoLoaded && logo.id != 0) {
        float iconSize = 24.0f;
        DrawTexturePro(logo, {0, 0, (float)logo.width, (float)logo.height},
                       {modal.x + 10, modal.y + 8, iconSize, iconSize}, {0, 0}, 0.0f, WHITE);
        titleX += iconSize + 8;
    }
    DrawText("Project Browser", (int)titleX, (int)modal.y + 10, 20, WHITE);

    Rectangle newBtn = { (float)(int)modal.x + 10, (float)(int)modal.y + 40, 120, 25 };
    if (GuiButton(newBtn, "New Project")) {
        wantsCreate = true;
    }

    Rectangle txtBox = { (float)(int)modal.x + 140, (float)(int)modal.y + 40, 250, 25 };
    if (wantsCreate) {
        GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
        GuiTextBox(txtBox, newProjectName, 256, true);
        GuiSetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    }

    float y = (int)modal.y + 80;
    for (size_t i = 0; i < projects.size() && y < (int)modal.y + 260; i++) {
        Rectangle btn = { (float)(int)modal.x + 10, (float)(int)y, 380, 25 };
        std::string label = projects[i].name + (projects[i].exists ? "" : " (missing .starproject)");
        if (GuiButton(btn, label.c_str())) {
            if (projects[i].exists) {
                selectedPath = projects[i].path;
                openProject = true;
            }
        }
        y += 30;
    }

    Rectangle cancelBtn = { (float)(int)modal.x + 10, (float)(int)modal.y + 270, 100, 25 };
    if (showCancel && GuiButton(cancelBtn, "Cancel")) {
        visible = false;
        wantsCancel = true;
    }

    bool hasName = strlen(newProjectName) > 0;
    bool nameIsUnique = hasName && IsNameUnique(std::string(newProjectName));
    Rectangle createBtn = { (float)(int)modal.x + 290, (float)(int)modal.y + 270, 100, 25 };
    if (nameIsUnique && GuiButton(createBtn, "Create")) {
        createConfirmed = true;
        wantsCreate = false;
    }
}