#pragma once
#include "EditorTab.h"
#include <vector>
#include <string>
#include <memory>

class LevelEditorTab;
class MenuBar;
class ProjectBrowser;
class LogPanel;
class AboutPanel;

class Editor {
public:
    void Initialize();
    ~Editor();
    void Render();

    LogPanel* GetLogPanel() const;

    void ShowProjectBrowser(bool forNew = false, bool allowCancel = false);
    bool WantsNewProject();
    bool WantsOpenProject();
    bool WantsSaveProject();
    bool WantsCancel();
    std::string GetNewProjectName() const;
    std::string GetOpenProjectPath() const;
    void ResetNewProjectFlag();
    void ResetSaveProjectFlag();

    void SaveLayout() const;
    void LoadLayout();

    void OpenDefaultMapIfSet();

private:
    void RenderEditorTabBar(float x, float y, float width);
    void OpenAssetInTab(const std::string& fullPath, const std::string& extension);
    void OpenProjectSettingsTab();
    void HandleWindowMenuAction(const std::string& item);
    void HandleEditMenuAction(const std::string& item);
    void HandleHelpMenuAction(const std::string& item);

    std::string GetLayoutPath() const;

    static const float EDITOR_TAB_BAR_H;
    static const float MENU_BAR_H;

    std::vector<std::unique_ptr<EditorTab>> editorTabs;
    int activeEditorTab = 0;

    LevelEditorTab* levelTab = nullptr;
    MenuBar*        menuBar  = nullptr;
    ProjectBrowser* projectBrowser = nullptr;
};