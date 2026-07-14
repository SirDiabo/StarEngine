#pragma once
#include "raylib.h"
#include "AssetPickerPopup.h"
#include "Core/StarAsset.h"
#include <string>
#include <vector>

class ImportDetailsPanel {
public:
    static ImportDetailsPanel& Get();

    void Open(const std::string& assetPath, const std::string& assetType);
    void Close();
    bool IsOpen() const { return open; }

    void Render();

private:
    void LoadFromAsset();
    void SaveToAsset();

    bool open = false;
    std::string assetPath;
    std::string assetType;

    char posBuf[3][32] = {};
    char rotBuf[3][32] = {};
    char scaleBuf[3][32] = {};
    int activeField = 0;

    std::vector<MaterialSlot> materialSlots;
    int materialSlotPickerIndex = -1;
    AssetPickerPopup materialPicker;
};
