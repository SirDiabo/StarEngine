#pragma once
#include "EditorTab.h"
#include "AssetPickerPopup.h"
#include "ChoicePopup.h"
#include "SettingsSwitcher.h"
#include "Core/SaveMigration.h"
#include <string>
#include <vector>

class MenuBar;
class Widget;
class TextElement;

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
    void RenderCollisionLayersCategory(Rectangle area);
    void RebuildLayerFields();
    void RenderMigrationSection(Rectangle area, float fx, float& fy, float fw);
    void RebuildMigrationFields();
    void CommitMigrations();

    SettingsSwitcher switcher;

    AssetPickerPopup mapPicker;
    AssetPickerPopup gameMapPicker;
    ChoicePopup rendererPopup;
    ChoicePopup shadingTypePopup;
    bool moduleSettingsPopupOpen = false;
    float moduleSettingsSectionHeight = 0.0f;

    std::vector<std::string> shadingOptionIds;

    char renderScaleBuf[16] = "";
    char newSaveFileBuf[64] = "";
    std::string newSaveFileName;
    int activeField = 0;

    Widget* renderScaleChrome = nullptr;
    TextElement* renderScaleField = nullptr;
    Widget* newSaveNameChrome = nullptr;
    TextElement* newSaveNameField = nullptr;

    std::vector<SaveMigration::MigrationStep> migrationSteps;
    int migrationVersion = 1;
    int selectedMigrationStep = -1;
    bool migrationsLoaded = false;
    std::string migrationSignature;
    Widget* migrationChrome = nullptr;
    std::vector<TextElement*> migrationFields;

    Widget* layerChrome = nullptr;
    std::vector<TextElement*> layerFields;
    std::vector<int> layerFieldIndices;
    Widget* newLayerChrome = nullptr;
    TextElement* newLayerField = nullptr;
    std::string newLayerName;
};
