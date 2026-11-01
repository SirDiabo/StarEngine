#pragma once
#include "Editor/EditorTab.h"
#include "Editor/DockArea.h"
#include <string>

class SequenceEditorSession;
class PianoRollPanel;
class SequenceLayersPanel;
class LogPanel;
class MenuBar;

class SequenceEditorTab : public EditorTab {
public:
    explicit SequenceEditorTab(const std::string& assetPath);
    ~SequenceEditorTab() override;

    void Initialize() override;
    void SetSharedLogPanel(LogPanel* shared) { logPanel = shared; }
    void Render(float x, float y, float width, float height) override;
    void RenderOverlays() override;
    void BuildMenus(MenuBar* menuBar) override;
    void OnDeactivated() override;
    bool IsOpen() const override;
    bool IsDirty() const override;

    std::string GetAssetPath() const override { return assetPath; }

    void ResetLayout() override;
    void TogglePanelVisible(const std::string& name) override;
    void SaveLayout(const std::string& filePath) const override;
    void LoadLayout(const std::string& filePath) override;

private:
    void BuildDefaultLayout();
    void RenderToolbar(Rectangle area);

    std::string assetPath;
    DockArea dockArea;

    SequenceEditorSession* session = nullptr;
    PianoRollPanel* pianoRollPanel = nullptr;
    SequenceLayersPanel* layersPanel = nullptr;
    LogPanel* logPanel = nullptr;
    bool opened = false;
};
