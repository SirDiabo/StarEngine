#pragma once
#include "Core/Panel.h"
#include <vector>
#include <string>

class LogPanel;
class ContentBrowserPanel;
class MenuBar;
class ProjectBrowser;

class Editor {
public:
    void Initialize();
    void Render();
    LogPanel* GetLogPanel();

    void ShowProjectBrowser();
    bool WantsNewProject();
    bool WantsOpenProject();
    bool WantsSaveProject();
    std::string GetNewProjectName() const;
    std::string GetOpenProjectPath() const;
    void ResetNewProjectFlag();

    bool CheckProjectBrowserCreate();
    std::string GetProjectBrowserNewName() const;
    void ResetProjectBrowserCreate();

private:
    std::vector<Panel*> panels;
    LogPanel* logPanel = nullptr;
    ContentBrowserPanel* contentPanel = nullptr;
    MenuBar* menuBar = nullptr;
    ProjectBrowser* projectBrowser = nullptr;
    std::vector<std::string> bottomTabs;
    int activeBottomTab = 0;
    std::string newProjectName;
};