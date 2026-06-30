#pragma once
#include "EditorTab.h"
#include <string>

class StargraphEditorPanel;
class LogPanel;
class MenuBar;

class StargraphEditorTab : public EditorTab {
public:
    explicit StargraphEditorTab(const std::string& assetPath);
    ~StargraphEditorTab() override;

    void Initialize() override;
    void SetSharedLogPanel(LogPanel* shared) { logPanel = shared; }
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;
    bool IsOpen() const override;

    const std::string& GetAssetPath() const { return assetPath; }

private:
    std::string assetPath;
    StargraphEditorPanel* panel = nullptr;
    LogPanel* logPanel = nullptr;
    float logSplitRatio = 0.75f;

    bool resizingSplitter = false;
    float splitterDragStart = 0.0f;
    float ratioAtDragStart = 0.0f;
};