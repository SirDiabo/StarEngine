#include "Editor.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "MenuBar.h"
#include "ProjectBrowser.h"
#include "raygui.h"
#include <algorithm>

static float lastClickTime = 0;
static int lastClickedTab = -1;

void Editor::Initialize() {
    bottomTabs = {"Logs", "Content"};

    logPanel = new LogPanel();
    logPanel->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(logPanel);

    contentPanel = new ContentBrowserPanel();
    contentPanel->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(contentPanel);

    DetailsPanel* details = new DetailsPanel();
    details->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(details);

    ViewportPanel* viewport = new ViewportPanel();
    viewport->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(viewport);

    menuBar = new MenuBar();
    menuBar->Initialize();

    projectBrowser = new ProjectBrowser();

    for (auto* panel : panels) {
        panel->Initialize();
    }
}

void Editor::Render() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    if (projectBrowser->IsVisible()) {
        projectBrowser->Render();
        return;
    }

    Vector2 mousePos = GetMousePosition();
    for (auto* panel : panels) {
        if (panel->IsDetached()) {
            if (panel->dragging) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    panel->bounds.x = mousePos.x - panel->dragOffset.x;
                    panel->bounds.y = mousePos.y - panel->dragOffset.y;
                    panel->header.x = panel->bounds.x;
                    panel->header.y = panel->bounds.y;
                } else {
                    panel->dragging = false;
                }
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mousePos, panel->header)) {
                    panel->dragging = true;
                    panel->dragOffset.x = mousePos.x - panel->bounds.x;
                    panel->dragOffset.y = mousePos.y - panel->bounds.y;
                }
            }
            panel->Render();
        }
    }

    float topHeight = height - 230;

    ViewportPanel* viewport = static_cast<ViewportPanel*>(panels[3]);
    viewport->bounds = Rectangle{0, 30, (float)(width - 300), topHeight};
    viewport->header = {viewport->bounds.x, viewport->bounds.y, viewport->bounds.width, 25};
    viewport->Render();

    DetailsPanel* details = static_cast<DetailsPanel*>(panels[2]);
    details->bounds = Rectangle{(float)(width - 300), 30, 300, topHeight};
    details->header = {details->bounds.x, details->bounds.y, details->bounds.width, 25};
    details->Render();

    Rectangle tabBarBounds = {0, (float)(height - 200), (float)width, 30};
    std::vector<const char*> tabTexts = {bottomTabs[0].c_str(), bottomTabs[1].c_str()};
    int prevTab = activeBottomTab;
    GuiTabBar(tabBarBounds, const_cast<char**>(tabTexts.data()), (int)bottomTabs.size(), &activeBottomTab);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, tabBarBounds)) {
        float currentTime = GetTime();
        if (lastClickedTab == activeBottomTab && (currentTime - lastClickTime) < 0.3f) {
            Panel* panel = (activeBottomTab == 0) ? static_cast<Panel*>(logPanel) : static_cast<Panel*>(contentPanel);
            if (!panel->IsDetached()) {
                panel->SetDetached(true);
                panel->bounds = Rectangle{200, 100, 500, 350};
            }
        }
        lastClickTime = currentTime;
        lastClickedTab = activeBottomTab;
    }

    Rectangle bottomBounds = {0, (float)(height - 170), (float)width, 170};
    if (activeBottomTab == 0) {
        logPanel->bounds = bottomBounds;
    } else {
        contentPanel->bounds = bottomBounds;
    }

    menuBar->Render();

    if (menuBar->activeMenu == 0) {
        Rectangle popup = {5, 35, 120, 90};
        Rectangle newBtn = {10, 40, 110, 25};
        Rectangle openBtn = {10, 70, 110, 25};
        Rectangle saveBtn = {10, 100, 110, 25};

        DrawRectangleRec(popup, {60, 60, 60, 255});
        if (GuiButton(newBtn, "New Project")) menuBar->wantsNewProject = true;
        if (GuiButton(openBtn, "Open Project")) menuBar->wantsOpenProject = true;
        if (GuiButton(saveBtn, "Save Project")) menuBar->wantsSaveProject = true;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            menuBar->activeMenu = -1;
        }
    }

    if (activeBottomTab == 0 && !logPanel->IsDetached()) {
        logPanel->Render();
    } else if (activeBottomTab == 1 && !contentPanel->IsDetached()) {
        contentPanel->Render();
    }
}

void Editor::ShowProjectBrowser(bool forNew, bool allowCancel) {
    projectBrowser->Show(forNew ? ProjectBrowser::Mode::Create : ProjectBrowser::Mode::Open, allowCancel);
}

bool Editor::IsProjectBrowserVisible() const {
    return projectBrowser && projectBrowser->IsVisible() && !projectBrowser->WantsNewProject() && !projectBrowser->WantsOpenProject();
}

bool Editor::WantsNewProject() {
    if (projectBrowser && projectBrowser->WantsNewProject()) {
        return true;
    }
    if (menuBar && menuBar->wantsNewProject) {
        ShowProjectBrowser(true, true);
        menuBar->wantsNewProject = false;
    }
    return false;
}

bool Editor::WantsOpenProject() {
    if (projectBrowser && projectBrowser->WantsOpenProject()) {
        return true;
    }
    if (menuBar && menuBar->wantsOpenProject) {
        ShowProjectBrowser(false, true);
        menuBar->wantsOpenProject = false;
    }
    return false;
}

bool Editor::WantsSaveProject() {
    return menuBar && menuBar->wantsSaveProject;
}

bool Editor::WantsCancel() {
    return projectBrowser && projectBrowser->WantsCancel();
}

std::string Editor::GetNewProjectName() const {
    if (projectBrowser && projectBrowser->WantsNewProject()) {
        return projectBrowser->GetNewProjectName();
    }
    return "";
}

std::string Editor::GetOpenProjectPath() const {
    return projectBrowser ? projectBrowser->GetSelectedPath() : "";
}

void Editor::ResetNewProjectFlag() {
    if (menuBar) menuBar->wantsNewProject = false;
    projectBrowser->ResetFlags();
}