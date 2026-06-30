#pragma once
#include "Core/Panel.h"
#include "Core/Module.h"
#include <string>

class LogPanel;

class ModuleBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }

private:
    void RenderRow(const ModuleEntry& entry, float y, float rowHeight);
    void ImportModule();

    LogPanel* logPanel = nullptr;
    std::string statusMessage;
    float scrollOffset = 0.0f;
};