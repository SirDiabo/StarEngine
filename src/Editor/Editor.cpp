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

    if (projectBrowser->IsVisible()) {
        return;
    }

    float topHeight = height - 230;

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

bool Editor::IsProjectBrowserVisible() const {
    return projectBrowser && projectBrowser->WantsNewProject() == false && projectBrowser->WantsOpenProject() == false;
}

bool Editor::WantsNewProject() {
    if (projectBrowser && projectBrowser->WantsNewProject()) {
        return true;
    }
    return menuBar && menuBar->wantsNewProject;
}

bool Editor::WantsOpenProject() {
    return projectBrowser && projectBrowser->WantsOpenProject();
}

bool Editor::WantsSaveProject() {
    return menuBar && menuBar->wantsSaveProject;
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
}