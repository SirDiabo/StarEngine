#include "ProjectBrowser.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

void ProjectBrowser::Show() {
    visible = true;
    wantsCreate = false;
    openProject = false;
    ScanProjects();
}

void ProjectBrowser::ScanProjects() {
    projects.clear();
    char* userProfile = getenv("USERPROFILE");
    fs::path docsPath = userProfile ? userProfile : "";
    docsPath /= "Documents";
    docsPath /= "StarProjects";

    if (fs::exists(docsPath)) {
        for (const auto& entry : fs::directory_iterator(docsPath)) {
            if (entry.is_directory()) {
                fs::path projFile = entry.path();
                projFile += "/" + entry.path().filename().string() + ".starproject";
                ProjectInfo info;
                info.name = entry.path().filename().string();
                info.path = projFile.string();
                info.exists = fs::exists(projFile);
                projects.push_back(info);
            }
        }
    }
}

void ProjectBrowser::Render() {
    if (!visible) return;

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    Rectangle modal = { (float)width/2 - 200, (float)height/2 - 150, 400, 300 };

    DrawRectangle(0, 0, width, height, Fade(BLACK, 0.5f));
    DrawRectangleRec(modal, {40, 40, 40, 255});
    DrawRectangleLinesEx(modal, 1, GRAY);

    DrawText("Projects", (int)modal.x + 10, (int)modal.y + 10, 20, WHITE);

    Rectangle newBtn = { (int)modal.x + 10, (int)modal.y + 40, 120, 25 };
    if (GuiButton(newBtn, "New Project")) {
        wantsCreate = true;
    }

    Rectangle txtBox = { (int)modal.x + 140, (int)modal.y + 40, 150, 25 };
    if (wantsCreate) {
        GuiTextBox(txtBox, newProjectName, 256, true);
    }

    float y = (int)modal.y + 80;
    for (const auto& proj : projects) {
        Rectangle btn = { (int)modal.x + 10, (int)y, 380, 25 };
        std::string label = proj.name + (proj.exists ? "" : " (missing .starproject)");
        if (GuiButton(btn, label.c_str())) {
            if (proj.exists) {
                selectedPath = proj.path;
                openProject = true;
            }
        }
        y += 30;
    }

    Rectangle createBtn = { (int)modal.x + 10, (int)modal.y + 270, 100, 25 };
    if (GuiButton(createBtn, "Create")) {
        wantsCreate = false;
    }
    Rectangle cancelBtn = { (int)modal.x + 290, (int)modal.y + 270, 100, 25 };
    if (GuiButton(cancelBtn, "Cancel")) {
        visible = false;
    }
}