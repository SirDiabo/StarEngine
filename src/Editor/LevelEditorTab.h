#pragma once
#include "EditorTab.h"
#include "DockArea.h"
#include <string>
#include <functional>

class ViewportPanel;
class DetailsPanel;
class OutlinerPanel;
class ContentBrowserPanel;
class LogPanel;
class ModuleBrowserPanel;
class ProfilerPanel;
class PlaceActorsPanel;
class MenuBar;
class FunctionUndoDocument;

class LevelEditorTab : public EditorTab {
public:
    ~LevelEditorTab() override;
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void RenderOverlays() override;
    void BuildMenus(MenuBar* menuBar) override;

    LogPanel*            GetLogPanel()     const { return logPanel; }
    ViewportPanel*       GetViewport()     const { return viewportPanel; }
    bool IsDirty() const override;
    ContentBrowserPanel* GetContentPanel() const { return contentPanel; }

    bool HasUnsavedLevelChanges() const;
    void SaveLevel();

    void TogglePanelVisible(const std::string& name) override;

    void SetPackageCallback(std::function<void()> callback);

    // Called by Editor when an asset drop happens over the viewport area.
    std::function<void(const std::string&, const std::string&)> onAssetOpened;

    void SaveLayout(const std::string& filePath) const override;
    void LoadLayout(const std::string& filePath) override;
    void SaveLayoutAs();
    void LoadLayoutFromDialog();
    void ResetLayout() override;

private:
    void BuildDefaultLayout();

    DockArea dockArea;
    FunctionUndoDocument* undoDocument = nullptr;

    ViewportPanel*       viewportPanel  = nullptr;
    DetailsPanel*        detailsPanel   = nullptr;
    OutlinerPanel*       outlinerPanel  = nullptr;
    ContentBrowserPanel* contentPanel   = nullptr;
    LogPanel*            logPanel       = nullptr;
    ModuleBrowserPanel*  modulePanel    = nullptr;
    ProfilerPanel*       profilerPanel  = nullptr;
    PlaceActorsPanel*    placeActorsPanel = nullptr;
};
