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

    // Lets the details panel ask the editor to open a class asset (.staractor) for
    // editing when the user clicks "Edit Class" on a placed instance.
    std::function<void(const std::string&, const std::string&)> onOpenAsset;

private:
    void RefreshFromSelection(const ActorInstance& inst);
    void RenderStaticMeshSection(ActorInstance& inst, float x, float& y, float w);
    void RenderClassComponentsSection(const ActorInstance& inst, float x, float& y, float w);

    char nameEditBuf[128] = "";
    char posXBuf[32] = "", posYBuf[32] = "", posZBuf[32] = "";
    char rotXBuf[32] = "", rotYBuf[32] = "", rotZBuf[32] = "";
    char scaleXBuf[32] = "", scaleYBuf[32] = "", scaleZBuf[32] = "";
    int lastActorIndex = -1;
    int activeField = 0;

    AssetPickerPopup meshPicker;
    AssetPickerPopup materialPicker;

    bool classComponentsValid = false;
    Stargraph cachedClassGraph;
    std::string cachedClassPath;
};
