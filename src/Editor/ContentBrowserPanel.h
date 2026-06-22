#pragma once
#include "Core/Panel.h"
#include "Core/AssetTypeRegistry.h"
#include <string>
#include <vector>

struct ContentEntry {
    std::string displayName;
    std::string fullDiskName;
    bool isFolder = false;
    std::string typeName;
};

class ContentBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

private:
    std::string GetCurrentFolderPath() const;
    std::vector<ContentEntry> ScanCurrentFolder() const;
    void NavigateInto(const std::string& folderName);
    void NavigateUp();

    void BeginCreateFolder();
    void BeginCreateAsset(const AssetTypeInfo& info);
    void ConfirmPendingCreate();
    void CancelPendingCreate();

    std::string currentRelativeFolder;
    int selectedIndex = -1;
    float lastEntryClickTime = 0;
    int lastEntryClickIndex = -1;

    bool contextMenuOpen = false;
    Vector2 contextMenuPos{0, 0};

    bool isCreatingNewItem = false;
    bool isCreatingFolder = false;
    std::string pendingExtension;
    char nameBuffer[128] = "";
};
