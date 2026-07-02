#pragma once
#include "Core/Panel.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/StarMaterial.h"
#include "AssetPickerPopup.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct ContentEntry {
    std::string displayName;
    std::string fullDiskName;
    bool isFolder = false;
    std::string typeName;
    std::string extension;
};

class ContentBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ContentBrowserPanel() override;

    std::function<void(const std::string& fullPath, const std::string& extension)> onAssetOpened;
    std::function<void(const std::string& fullPath, const std::string& extension, Vector2 dropPos)> onAssetDragged;

private:
    std::string GetCurrentFolderPath() const;
    std::vector<ContentEntry> ScanCurrentFolder() const;
    void NavigateInto(const std::string& folderName);
    void NavigateUp();

    void BeginCreateFolder();
    void BeginCreateAsset(const AssetTypeInfo& info);
    void ConfirmPendingCreate();
    void CancelPendingCreate();
    void ImportAsset(const std::string& dialogTitle, const std::string& filterDesc,
                      const std::string& filterExt, const std::string& assetType,
                      const std::string& extension);
    void ImportStaticMesh();
    void ImportTexture();

    Texture2D GetIconForEntry(const ContentEntry& entry) const;
    void LoadIcons();
    void UnloadIcons();
    void OpenMaterialEditor(const std::string& fullPath);
    void RenderMaterialEditor();

    std::string currentRelativeFolder;
    int selectedIndex = -1;
    float lastEntryClickTime = 0;
    int lastEntryClickIndex = -1;

    bool contextMenuOpen = false;
    bool contextMenuJustOpened = false;
    Vector2 contextMenuPos{0, 0};

    bool draggingAsset = false;
    int dragEntryIndex = -1;
    Vector2 dragPressPos{0, 0};
    std::string dragEntryPath;
    std::string dragEntryExt;

    bool isCreatingNewItem = false;
    bool isCreatingFolder = false;
    std::string pendingExtension;
    char nameBuffer[128] = "";

    std::string importStatus;

    float tileSize = 80.0f;

    std::unordered_map<std::string, Texture2D> icons;
    bool iconsLoaded = false;

    bool materialEditorOpen = false;
    std::string materialEditorPath;
    StarMaterial materialEditorData;
    char materialColorBuf[4][8] = {"", "", "", ""};
    int materialActiveField = 0;
    AssetPickerPopup materialTexturePicker;
};