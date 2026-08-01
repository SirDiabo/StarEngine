#pragma once
#include "EditorTab.h"
#include "SettingsSwitcher.h"

class MenuBar;

class EditorSettingsTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

private:
    void RenderStyleCategory(Rectangle area);

    SettingsSwitcher switcher;
    float colorListScroll = 0.0f;
};