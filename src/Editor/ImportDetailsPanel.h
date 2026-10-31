#pragma once
#include "raylib.h"
#include "AssetPickerPopup.h"
#include "PropertyPanel.h"
#include "Core/StarAsset.h"
#include <string>
#include <vector>

class Widget;

class ImportDetailsPanel {
public:
    static ImportDetailsPanel& Get();

    int SettingsRevision() const { return settingsRevision; }

    bool IsPendingImport() const { return pendingImport; }
    bool IsShowingInline(const std::string& path) const { return open && inlinePanel && !pendingImport && assetPath == path; }
    void EnsureInline(const std::string& path, const std::string& type);
    void ReleaseInline(const std::string& path);

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
    std::vector<PropertyRow> BuildRows(bool modal);
    void EnsureChrome();
    void ApplyPanelTheme();
    void ApplyImportOrSave();

    int settingsRevision = 0;
    bool open = false;
    bool inlinePanel = false;
    std::string assetPath;
    std::string assetType;

    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;

    std::vector<MaterialSlot> materialSlots;
    int materialSlotPickerIndex = -1;
    AssetPickerPopup materialPicker;

    bool hasSkeleton = false;
    int skeletonBoneCount = 0;

    std::string soundLoadMode = "Auto";
    std::string soundBus;
    float soundVolume = 1.0f;
    float soundPitch = 1.0f;
    std::string soundInfoText;

    bool pendingImport = false;
    std::string pendingSrcPath;
    std::string pendingExtension;
    std::string pendingTargetFolder;

    bool hasAnimations = false;
    std::vector<std::string> pendingAnimationNames;
    bool animationOnly = false;
    std::string animationOnlyTargetMesh;
    std::vector<std::string> compatibleMeshPaths;

    PropertyPanel* panel = nullptr;
    Widget* chrome = nullptr;
};
