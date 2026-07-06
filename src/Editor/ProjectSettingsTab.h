#pragma once
#include "EditorTab.h"
#include "AssetPickerPopup.h"
#include "ChoicePopup.h"

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

    char widthBuf[16]  = "";
    char heightBuf[16] = "";
    int activeField = 0;
};
