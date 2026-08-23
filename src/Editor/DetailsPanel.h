#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "AssetPickerPopup.h"
#include "Modules/Stargraph/Stargraph.h"
#include <functional>

class DetailsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    const StarMap* starMap = nullptr;
    int* selectedActorIndex = nullptr;

    std::function<void(const std::string&, const std::string&)> onOpenAsset;

    std::function<void()> onEdited;

private:
    void RefreshFromSelection(const ActorInstance& inst);
    void RenderStaticMeshSection(ActorInstance& inst, float x, float& y, float w);
    void RenderClassComponentsSection(const ActorInstance& inst, float x, float& y, float w);
    void RenderInlineComponentSection(ActorInstance& inst, float x, float& y, float w);

    char nameEditBuf[128] = "";
    char posXBuf[32] = "", posYBuf[32] = "", posZBuf[32] = "";
    char rotXBuf[32] = "", rotYBuf[32] = "", rotZBuf[32] = "";
    char scaleXBuf[32] = "", scaleYBuf[32] = "", scaleZBuf[32] = "";
    int lastActorIndex = -1;
    int activeField = 0;

    AssetPickerPopup meshPicker;
    AssetPickerPopup materialPicker;
    int materialSlotPickerIndex = -1;

    char compLightColorBuf[3][32] = {"", "", ""};
    char compLightIntensityBuf[32] = "";
    char compLightRadiusBuf[32] = "";
    char compLightInnerConeBuf[32] = "";
    char compLightOuterConeBuf[32] = "";
    char compLightWidthBuf[32] = "";
    char compLightHeightBuf[32] = "";
    char compCameraFovBuf[32] = "";
    char compCameraNearBuf[32] = "";
    char compCameraFarBuf[32] = "";
    char compAudioVolumeBuf[32] = "";
    char compAudioPitchBuf[32] = "";
    AssetPickerPopup audioPathPicker;
};
