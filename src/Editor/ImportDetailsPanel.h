#pragma once
#include "raylib.h"
#include "AssetPickerPopup.h"
#include <string>

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

    std::string defaultMaterialPath;
    AssetPickerPopup materialPicker;
};
