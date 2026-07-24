#pragma once
#include "EditorTab.h"
#include "AssetPickerPopup.h"
#include "ChoicePopup.h"
#include <string>

class MenuBar;

class ProjectSettingsTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

private:
    void RefreshFromProject();

    AssetPickerPopup mapPicker;
    ChoicePopup rendererPopup;
    ChoicePopup shadingTypePopup;

    char renderScaleBuf[16] = "";
    char newSaveFileBuf[64] = "";
    std::string newSaveFileName;
    int activeField = 0;
};