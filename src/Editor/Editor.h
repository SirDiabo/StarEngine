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

    void ShowProjectBrowser(bool forNew = false, bool allowCancel = false);
    bool IsProjectBrowserVisible() const;
    bool WantsNewProject();
    bool WantsOpenProject();
    bool WantsSaveProject();
    bool WantsCancel();
    std::string GetNewProjectName() const;
    std::string GetOpenProjectPath() const;
    void ResetNewProjectFlag();

private:
    std::vector<Panel*> panels;
    LogPanel* logPanel = nullptr;
    ContentBrowserPanel* contentPanel = nullptr;
    MenuBar* menuBar = nullptr;
    ProjectBrowser* projectBrowser = nullptr;
    std::vector<std::string> bottomTabs;
    int activeBottomTab = 0;
};