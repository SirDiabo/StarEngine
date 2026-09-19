#pragma once
#include "Core/Panel.h"
#include "AssetPickerPopup.h"
#include "PropertyPanel.h"
#include <functional>
#include <string>
#include <vector>

class Widget;

class DetailsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~DetailsPanel() override;

    std::function<std::vector<PropertyRow>()> buildRows;
    std::function<void(const std::string& rowId, const std::string& pickedPath)> onAssetPicked;

private:
    Widget* chrome = nullptr;
    PropertyPanel* panelElement = nullptr;

    AssetPickerPopup assetPicker;
    std::string pendingAssetRowId;
};
