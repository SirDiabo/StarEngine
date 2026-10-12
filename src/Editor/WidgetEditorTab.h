#pragma once
#include "Editor/EditorTab.h"
#include "Editor/DockArea.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

class WidgetEditorSession;
class WidgetPalettePanel;
class WidgetHierarchyPanel;
class WidgetCanvasPanel;
class DetailsPanel;
class WidgetDetailsSwitch;
class LogPanel;
class MenuBar;
class ViewportPanel;
class Widget;
class Panel;
struct DockNode;

class WidgetEditorTab : public EditorTab {
public:
    explicit WidgetEditorTab(const std::string& assetPath);
    ~WidgetEditorTab() override;

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
    void BuildTopBarChrome();
    void ApplyDesignerLayout();
    bool ApplyGraphLayout();
    void Save();

    std::string assetPath;
    DockArea dockArea;

    WidgetEditorSession* session = nullptr;
    WidgetPalettePanel* palettePanel = nullptr;
    WidgetHierarchyPanel* hierarchyPanel = nullptr;
    WidgetCanvasPanel* canvasPanel = nullptr;
    DetailsPanel* detailsPanel = nullptr;
    WidgetDetailsSwitch* detailsSwitch = nullptr;
    LogPanel* logPanel = nullptr;
    ViewportPanel* viewportPanel = nullptr;

    std::vector<Panel*> graphPanels;
    std::function<void()> graphTeardown;
    std::function<void(const std::string&, const std::string&)> graphElementDragCallback;
    std::unique_ptr<DockNode> designerLayoutSnapshot;
    std::unique_ptr<DockNode> designerDefaultLayout;
    std::unique_ptr<DockNode> graphLayoutSnapshot;
    std::unique_ptr<DockNode> graphDefaultLayout;
    std::string graphModeLabel = "Graph";
    bool graphPanelsBuilt = false;
    bool inGraphMode = false;

    Widget* topBarChrome = nullptr;
};
