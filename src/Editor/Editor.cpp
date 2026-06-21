#include "Editor.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "MenuBar.h"
#include "ProjectBrowser.h"
#include "raygui.h"

static float lastClickTime = 0;
static int lastClickedTab = -1;

void Editor::Initialize() {
    contentPanel = new ContentBrowserPanel();
    panels.push_back(contentPanel);

    logPanel = new LogPanel();
    panels.push_back(logPanel);

    detailsPanel = new DetailsPanel();
    panels.push_back(detailsPanel);

    viewportPanel = new ViewportPanel();
    panels.push_back(viewportPanel);

    bottomDock = { {contentPanel, "Content"}, {logPanel, "Logs"} };

    menuBar = new MenuBar();
    menuBar->Initialize();

    projectBrowser = new ProjectBrowser();

    for (auto* panel : panels) {
        panel->Initialize();
    }
}

LogPanel* Editor::GetLogPanel() {
    return logPanel;
}

void Editor::Render() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    if (projectBrowser->IsVisible()) {
        projectBrowser->Render();
        return;
    }

    Rectangle tabBarBounds = {0, (float)(height - 200), (float)width, 30};
    Vector2 mousePos = GetMousePosition();
    int reattachIndex = -1;

    for (size_t i = 0; i < bottomDock.size(); i++) {
        Panel* panel = bottomDock[i].panel;
        if (!panel->IsDetached()) continue;

        if (panel->dragging) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                panel->bounds.x = mousePos.x - panel->dragOffset.x;
                panel->bounds.y = mousePos.y - panel->dragOffset.y;
                panel->header.x = panel->bounds.x;
                panel->header.y = panel->bounds.y;
            } else {
                panel->dragging = false;
                if (CheckCollisionPointRec(mousePos, tabBarBounds)) {
                    panel->SetDetached(false);
                    reattachIndex = (int)i;
                }
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, panel->header)) {
            panel->dragging = true;
            panel->dragOffset.x = mousePos.x - panel->bounds.x;
            panel->dragOffset.y = mousePos.y - panel->bounds.y;
        }

        panel->Render();
    }

    float topHeight = height - 230;

    viewportPanel->bounds = Rectangle{0, 30, (float)(width - 300), topHeight};
    viewportPanel->header = {viewportPanel->bounds.x, viewportPanel->bounds.y, viewportPanel->bounds.width, 25};
    viewportPanel->Render();

    detailsPanel->bounds = Rectangle{(float)(width - 300), 30, 300, topHeight};
    detailsPanel->header = {detailsPanel->bounds.x, detailsPanel->bounds.y, detailsPanel->bounds.width, 25};
    detailsPanel->Render();

    std::vector<const char*> tabTexts;
    for (const auto& tab : bottomDock) tabTexts.push_back(tab.title.c_str());
    GuiTabBar(tabBarBounds, const_cast<char**>(tabTexts.data()), (int)bottomDock.size(), &activeBottomTab);

    if (reattachIndex >= 0) activeBottomTab = reattachIndex;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, tabBarBounds)) {
        float currentTime = GetTime();
        if (lastClickedTab == activeBottomTab && (currentTime - lastClickTime) < 0.3f) {
            Panel* panel = bottomDock[activeBottomTab].panel;
            if (!panel->IsDetached()) {
                panel->SetDetached(true);
                panel->bounds = Rectangle{200, 100, 500, 350};
            }
        }
        lastClickTime = currentTime;
        lastClickedTab = activeBottomTab;
    }

    Panel* activeBottomPanel = bottomDock[activeBottomTab].panel;
    if (!activeBottomPanel->IsDetached()) {
        activeBottomPanel->bounds = Rectangle{0, (float)(height - 170), (float)width, 170};
        activeBottomPanel->Render();
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
    } else if (menuBar->activeMenu > 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            menuBar->activeMenu = -1;
        }
    }
}

void Editor::ShowProjectBrowser(bool forNew, bool allowCancel) {
    projectBrowser->Show(forNew ? ProjectBrowser::Mode::Create : ProjectBrowser::Mode::Open, allowCancel);
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

void Editor::ResetSaveProjectFlag() {
    if (menuBar) menuBar->wantsSaveProject = false;
}
