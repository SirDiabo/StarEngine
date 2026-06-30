#pragma once
#include "Core/Panel.h"
#include <string>

class ModuleBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

private:
    void RenderRow(const struct ModuleEntry& entry, float y, float rowHeight);
    void ImportModule();

    float scrollOffset = 0.0f;
    std::string statusMessage;
};
