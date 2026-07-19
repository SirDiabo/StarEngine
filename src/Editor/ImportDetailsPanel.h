#pragma once
#include "raylib.h"
#include "AssetPickerPopup.h"
#include "Core/StarAsset.h"
#include <string>
#include <vector>

class ImportDetailsPanel {
public:
    static ImportDetailsPanel& Get();

    void Open(const std::string& assetPath, const std::string& assetType, bool inlinePanel = false);
    void Close();
    bool IsOpen() const { return open; }

    void Render();
    void RenderInline(Rectangle area);
    void ApplyAndClose();

private:
    void LoadFromAsset();
    void SaveToAsset();
    void RenderBody(Rectangle box, float boxWidth, bool modal);

    bool open = false;
    bool inlinePanel = false;
    std::string assetPath;
    std::string assetType;

    char posBuf[3][32] = {};
    char rotBuf[3][32] = {};
    char scaleBuf[3][32] = {};
    int activeField = 0;
    int lastActiveField = 0;
    bool pendingSave = false;

    std::vector<MaterialSlot> materialSlots;
    int materialSlotPickerIndex = -1;
    AssetPickerPopup materialPicker;
};
