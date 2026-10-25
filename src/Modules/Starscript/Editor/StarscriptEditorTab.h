#pragma once
#include "Editor/EditorTab.h"
#include "Editor/DockArea.h"
#include <string>

class StarscriptEditorSession;
class StarscriptScriptPanel;
class StarscriptOutlinePanel;
class ComponentOutlinerPanel;
class ComponentDetailsPanel;
class ActorPreviewPanel;
class LogPanel;
class MenuBar;
class ViewportPanel;

class StarscriptEditorTab : public EditorTab {
public:
    explicit StarscriptEditorTab(const std::string& assetPath);
    ~StarscriptEditorTab() override;

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

    StarscriptEditorSession* session = nullptr;
    ComponentOutlinerPanel* componentsPanel = nullptr;
    StarscriptOutlinePanel* outlinePanel = nullptr;
    StarscriptScriptPanel* scriptPanel = nullptr;
    ComponentDetailsPanel* detailsPanel = nullptr;
    ActorPreviewPanel* previewPanel = nullptr;
    LogPanel* logPanel = nullptr;
    ViewportPanel* viewportPanel = nullptr;
    bool opened = false;
};
