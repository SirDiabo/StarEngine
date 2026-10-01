#pragma once
#include "Core/Panel.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/StarMaterial.h"
#include "AssetPickerPopup.h"
#include "ColorPickerPopup.h"
#include "ConfirmPopup.h"
#include "ChoicePopup.h"
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

class Widget;
class VerticalBoxElement;
class HorizontalBoxElement;
class BoxElement;
class GridElement;
class ButtonElement;
class CheckBoxElement;

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
    void NavigateToRelativeFolder(const std::string& relativePath);

    void BeginCreateFolder();
    void BeginCreateAsset(const AssetTypeInfo& info);
    void ConfirmPendingCreate();
    void CancelPendingCreate();
    void CookImportedAsset(const std::string& srcPath, const std::string& assetType,
                            const std::string& extension);
    void BeginMeshImport(const std::string& srcPath);
    void ImportAssetAuto();
    void ImportPathAuto(const std::string& path);
    void ReimportFromFilePrompt(const std::string& starAssetPath, const std::string& displayName);
    void HandleFileDrop();

    void OpenEntry(const ContentEntry& entry);
    void BeginRename(const ContentEntry& entry);
    void ConfirmRename();
    void CancelRename();
    void DuplicateEntry(const std::string& fullPath, bool isFolder);
    void CopyToClipboard(const std::string& fullPath, bool isFolder);
    void PasteFromClipboard();
    void RequestDelete(const std::string& fullPath, bool isFolder, const std::string& displayName);
    void RevealInExplorer(const std::string& fullPath, bool isFolder);

    Texture2D GetIconForEntry(const ContentEntry& entry) const;
    const Texture2D* GetIconTexturePtr(const ContentEntry& entry) const;
    void BuildChrome();
    void RebuildBreadcrumb(const std::vector<std::pair<std::string, std::string>>& crumbs);
    void RebuildTiles(const std::vector<ContentEntry>& entries);
    void OpenItemContextMenu(Vector2 pos);
    void OpenGridContextMenu(Vector2 pos);
    void LoadIcons();
    void UnloadIcons();
    void OpenMaterialEditor(const std::string& fullPath);
    void RenderMaterialEditor();

    std::string currentRelativeFolder;
    int selectedIndex = -1;
    float lastEntryClickTime = 0;
    int lastEntryClickIndex = -1;

    ChoicePopup gridContextMenu;
    ChoicePopup itemContextMenu;
    std::vector<const AssetTypeInfo*> gridMenuTypes;
    std::vector<std::string> itemMenuActions;

    struct TileRef {
        ButtonElement* button = nullptr;
        std::string diskName;
        bool isFolder = false;
    };

    Widget* chrome = nullptr;
    VerticalBoxElement* chromeRoot = nullptr;
    HorizontalBoxElement* breadcrumbBar = nullptr;
    BoxElement* gridScroll = nullptr;
    GridElement* tileGrid = nullptr;
    std::vector<TileRef> tileButtons;
    std::string lastBuiltFolder = "\x01";
    std::string lastTileSignature;
    bool tilesDirty = false;

    std::string contextTargetPath;
    std::string contextTargetDisplayName;
    std::string contextTargetExtension;
    bool contextTargetIsFolder = false;

    bool isRenamingItem = false;
    std::string renamingOriginalDiskName;
    std::string renamingExtension;
    bool renamingIsFolder = false;

    std::string clipboardPath;
    bool clipboardIsFolder = false;

    ConfirmPopup deleteConfirm;
    std::string pendingDeletePath;
    bool pendingDeleteIsFolder = false;

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
    AssetPickerPopup materialTexturePicker;
    ColorPickerPopup materialColorPicker;

    void BuildMaterialEditorChrome();
    Widget* materialChrome = nullptr;
    TextElement* materialTitle = nullptr;
    TextElement* materialChannelBoxes[4] = {nullptr, nullptr, nullptr, nullptr};
    RectangleElement* materialSwatch = nullptr;
    TextElement* materialLitLabel = nullptr;
    CheckBoxElement* materialLitBox = nullptr;
    TextElement* materialTexLabel = nullptr;
    Vector2 swatchBtn2Pos{0, 0};
    Vector2 texRowScreenPos{0, 0};

    Widget* createNameChrome = nullptr;
    TextElement* createNameBox = nullptr;
};
