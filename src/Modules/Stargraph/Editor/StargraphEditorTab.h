#pragma once
#include "Editor/EditorTab.h"
#include "Editor/DockArea.h"
#include <string>

class StargraphEditorPanel;
class ComponentOutlinerPanel;
class StargraphPanel;
class StargraphGraphPanel;
class StargraphDetailsPanel;
class ActorPreviewPanel;
class LogPanel;
class MenuBar;
class ViewportPanel;

class StargraphEditorTab : public EditorTab {
public:
    explicit StargraphEditorTab(const std::string& assetPath);
    ~StargraphEditorTab() override;

    void Initialize() override;
    void SetSharedLogPanel(LogPanel* shared) { logPanel = shared; }
    void SetViewportPanel(ViewportPanel* viewport) { viewportPanel = viewport; }
    void Render(float x, float y, float width, float height) override;
    void RenderOverlays() override;
    void BuildMenus(MenuBar* menuBar) override;
    bool IsOpen() const override;
    bool IsDirty() const override;

    std::string GetAssetPath() const override { return assetPath; }

    void ResetLayout() override;
    void TogglePanelVisible(const std::string& name) override;
    void SaveLayout(const std::string& filePath) const override;
    void LoadLayout(const std::string& filePath) override;

private:
    void BuildDefaultLayout();

    std::string assetPath;

    DockArea dockArea;

    StargraphEditorPanel*    panel          = nullptr;
    ComponentOutlinerPanel*  outlinerPanel  = nullptr;
    StargraphPanel*          stargraphPanel = nullptr;
    StargraphGraphPanel*     graphPanel     = nullptr;
    StargraphDetailsPanel*   detailsPanel   = nullptr;
    ActorPreviewPanel*       previewPanel   = nullptr;
    LogPanel* logPanel = nullptr;
    ViewportPanel* viewportPanel = nullptr;
};
