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
class MenuBar;

class LevelEditorTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void RenderOverlays();
    void BuildMenus(MenuBar* menuBar) override;

    LogPanel*            GetLogPanel()     const { return logPanel; }
    ViewportPanel*       GetViewport()     const { return viewportPanel; }
    ContentBrowserPanel* GetContentPanel() const { return contentPanel; }

    void TogglePanelVisible(const std::string& name);

    // Called by Editor when an asset drop happens over the viewport area.
    std::function<void(const std::string&, const std::string&)> onAssetOpened;

    void SaveLayout(const std::string& filePath) const;
    void LoadLayout(const std::string& filePath);
    void SaveLayoutAs();
    void LoadLayoutFromDialog();
    void ResetLayout();

private:
    void BuildDefaultLayout();

    DockArea dockArea;

    ViewportPanel*       viewportPanel  = nullptr;
    DetailsPanel*        detailsPanel   = nullptr;
    OutlinerPanel*       outlinerPanel  = nullptr;
    ContentBrowserPanel* contentPanel   = nullptr;
    LogPanel*            logPanel       = nullptr;
};