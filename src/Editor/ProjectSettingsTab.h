#pragma once
#include "EditorTab.h"
#include "AssetPickerPopup.h"
#include "ChoicePopup.h"
#include "SettingsSwitcher.h"
#include <string>
#include <vector>

class MenuBar;

class ProjectSettingsTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

private:
    void RefreshFromProject();

    void RenderGeneralCategory(Rectangle area);
    void RenderMapsCategory(Rectangle area);
    void RenderRenderingCategory(Rectangle area);
    void RenderSaveFilesCategory(Rectangle area);

    SettingsSwitcher switcher;

    AssetPickerPopup mapPicker;
    ChoicePopup rendererPopup;
    ChoicePopup shadingTypePopup;
    bool moduleSettingsPopupOpen = false;

    std::vector<std::string> shadingOptionIds;

    char renderScaleBuf[16] = "";
    char newSaveFileBuf[64] = "";
    std::string newSaveFileName;
    int activeField = 0;
};