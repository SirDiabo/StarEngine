#pragma once
#include "EditorTab.h"
#include "AssetPickerPopup.h"
#include "ChoicePopup.h"
#include "SettingsSwitcher.h"
#include "Core/SaveMigration.h"
#include <map>
#include <string>
#include <vector>

class MenuBar;
class Widget;
class TextElement;

class ProjectSettingsTab : public EditorTab {
public:
    ~ProjectSettingsTab() override;
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

private:
    void RefreshFromProject();

    void RenderGeneralCategory(Rectangle area);
    void RenderMapsCategory(Rectangle area);
    void RenderRenderingCategory(Rectangle area);
    void RenderSaveFilesCategory(Rectangle area);
    void RenderAudioCategory(Rectangle area);
    float RenderAudioEffectsSection(float fx, float fy, float fw, Rectangle view);
    float RenderAttenuationSection(float fx, float fy, float fw, Rectangle view);
    float RenderBusEffectChips(float fx, float fy, float fw, const std::string& busName);
    void RenderCollisionLayersCategory(Rectangle area);
    void RebuildLayerFields();
    void RenderMigrationSection(Rectangle area, float fx, float& fy, float fw);
    void RebuildMigrationFields();
    void CommitMigrations();

    SettingsSwitcher switcher;

    Texture2D iconPreview{};
    std::string iconPreviewSource;
    std::string iconError;

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
    Widget* newBusChrome = nullptr;
    TextElement* newBusField = nullptr;
    std::string newBusName;

    enum class EffectPopupMode { None, NewType, ParamChoice, BusAdd, AttenuationChoice, AttenuationEffect };
    ChoicePopup effectPopup;
    EffectPopupMode effectPopupMode = EffectPopupMode::None;
    std::string effectPopupBus;
    std::string effectPopupParam;
    std::vector<std::string> effectPopupValues;
    std::string selectedEffect;
    std::string activeEffectSlider;
    std::string newEffectName;
    int newEffectType = 0;
    int lastCategory = -1;
    float audioScroll = 0.0f;
    float audioContentHeight = 0.0f;
    Widget* effectsChrome = nullptr;
    TextElement* newEffectField = nullptr;
    TextElement* renameEffectField = nullptr;
    std::string renameBound;
    std::string selectedAttenuation;
    std::string newAttenuationName;
    Widget* attenuationChrome = nullptr;
    TextElement* newAttenuationField = nullptr;
    TextElement* renameAttenuationField = nullptr;
    std::string attenuationRenameBound;

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
