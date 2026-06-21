#pragma once
#include "Core/Panel.h"
#include <vector>
#include <string>

class LogPanel;
class ContentBrowserPanel;
class DetailsPanel;
class ViewportPanel;
class MenuBar;
class ProjectBrowser;

struct DockTab {
    Panel* panel;
    std::string title;
};

class Editor {
public:
    void Initialize();
    void Render();
    LogPanel* GetLogPanel();

    void ShowProjectBrowser(bool forNew = false, bool allowCancel = false);
    bool WantsNewProject();
    bool WantsOpenProject();
    bool WantsSaveProject();
    bool WantsCancel();
    std::string GetNewProjectName() const;
    std::string GetOpenProjectPath() const;
    void ResetNewProjectFlag();
    void ResetSaveProjectFlag();

private:
    std::vector<Panel*> panels;
    std::vector<DockTab> bottomDock;
    int activeBottomTab = 0;

    LogPanel* logPanel = nullptr;
    ContentBrowserPanel* contentPanel = nullptr;
    DetailsPanel* detailsPanel = nullptr;
    ViewportPanel* viewportPanel = nullptr;
    MenuBar* menuBar = nullptr;
    ProjectBrowser* projectBrowser = nullptr;
};