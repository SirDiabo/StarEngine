#include "Editor.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "MenuBar.h"
#include "ProjectBrowser.h"
#include "raygui.h"

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

    menuBar->Render();
    projectBrowser->Render();

    float topHeight = height - 230;
    float bottomHeight = 200;

    ViewportPanel* viewport = static_cast<ViewportPanel*>(panels[3]);
    viewport->bounds = Rectangle{0, 30, (float)(width - 300), topHeight};
    viewport->Render();

    DetailsPanel* details = static_cast<DetailsPanel*>(panels[2]);
    details->bounds = Rectangle{(float)(width - 300), 30, 300, topHeight};
    details->Render();

    Rectangle tabBarBounds = {0, (float)(height - 200), (float)width, 30};
    std::vector<const char*> tabTexts = {bottomTabs[0].c_str(), bottomTabs[1].c_str()};
    GuiTabBar(tabBarBounds, const_cast<char**>(tabTexts.data()), (int)bottomTabs.size(), &activeBottomTab);

    Rectangle bottomBounds = {0, (float)(height - 170), (float)width, 170};
    if (activeBottomTab == 0) {
        logPanel->bounds = bottomBounds;
        logPanel->Render();
    } else {
        contentPanel->bounds = bottomBounds;
        contentPanel->Render();
    }
}

LogPanel* Editor::GetLogPanel() {
    return logPanel;
}

void Editor::ShowProjectBrowser() {
    projectBrowser->Show();
}

bool Editor::WantsNewProject() {
    return menuBar && menuBar->wantsNewProject;
}

bool Editor::WantsOpenProject() {
    return menuBar && menuBar->wantsOpenProject;
}

bool Editor::WantsSaveProject() {
    return menuBar && menuBar->wantsSaveProject;
}

std::string Editor::GetNewProjectName() const {
    return newProjectName;
}

std::string Editor::GetOpenProjectPath() const {
    return projectBrowser ? projectBrowser->GetSelectedPath() : "";
}

void Editor::ResetNewProjectFlag() {
    if (menuBar) menuBar->wantsNewProject = false;
}

bool Editor::CheckProjectBrowserCreate() {
    return projectBrowser && projectBrowser->WantsNewProject();
}

std::string Editor::GetProjectBrowserNewName() const {
    if (projectBrowser) return std::string(projectBrowser->GetNewProjectName());
    return "";
}

void Editor::ResetProjectBrowserCreate() {
    if (projectBrowser) projectBrowser->ClearCreateRequest();
}