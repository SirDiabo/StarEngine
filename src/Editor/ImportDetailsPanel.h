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
    void OpenForImport(const std::string& srcPath, const std::string& assetType, const std::string& extension,
                        const std::string& targetFolder);
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

    bool hasSkeleton = false;
    int skeletonBoneCount = 0;

    bool pendingImport = false;
    std::string pendingSrcPath;
    std::string pendingExtension;
    std::string pendingTargetFolder;

    bool hasAnimations = false;
    std::vector<std::string> pendingAnimationNames;
    bool animationOnly = false;
    std::string animationOnlyTargetMesh;
    std::vector<std::string> compatibleMeshPaths;
};
