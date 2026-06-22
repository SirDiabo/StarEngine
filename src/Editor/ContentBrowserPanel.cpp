#include "ContentBrowserPanel.h"
#include "Core/Project.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

void ContentBrowserPanel::Initialize() {
    name = "Content";
}

std::string ContentBrowserPanel::GetCurrentFolderPath() const {
    fs::path path = fs::path(Project::Get().GetRootPath()) / "Content";
    if (!currentRelativeFolder.empty()) path /= currentRelativeFolder;
    return path.string();
}

std::vector<ContentEntry> ContentBrowserPanel::ScanCurrentFolder() const {
    std::vector<ContentEntry> folders;
    std::vector<ContentEntry> files;

    fs::path folder = GetCurrentFolderPath();
    if (!fs::exists(folder)) return folders;

    for (const auto& entry : fs::directory_iterator(folder)) {
        ContentEntry item;
        item.fullDiskName = entry.path().filename().string();

        if (entry.is_directory()) {
            item.isFolder = true;
            item.displayName = item.fullDiskName;
            folders.push_back(item);
        } else {
            item.isFolder = false;
            item.displayName = entry.path().stem().string();
            std::string extension = entry.path().extension().string();
            const AssetTypeInfo* info = AssetTypeRegistry::Get().FindByExtension(extension);
            item.typeName = info ? info->typeName : extension;
            files.push_back(item);
        }
    }

    folders.insert(folders.end(), files.begin(), files.end());
    return folders;
}

void ContentBrowserPanel::NavigateInto(const std::string& folderName) {
    currentRelativeFolder = currentRelativeFolder.empty() ? folderName : currentRelativeFolder + "/" + folderName;
    selectedIndex = -1;
}

void ContentBrowserPanel::NavigateUp() {
    size_t separator = currentRelativeFolder.find_last_of('/');
    currentRelativeFolder = (separator == std::string::npos) ? "" : currentRelativeFolder.substr(0, separator);
    selectedIndex = -1;
}

void ContentBrowserPanel::BeginCreateFolder() {
    isCreatingNewItem = true;
    isCreatingFolder = true;
    pendingExtension.clear();
    snprintf(nameBuffer, sizeof(nameBuffer), "NewFolder");
    contextMenuOpen = false;
}

void ContentBrowserPanel::BeginCreateAsset(const AssetTypeInfo& info) {
    isCreatingNewItem = true;
    isCreatingFolder = false;
    pendingExtension = info.extension;
    snprintf(nameBuffer, sizeof(nameBuffer), "New%s", info.typeName.c_str());
    contextMenuOpen = false;
}

void ContentBrowserPanel::ConfirmPendingCreate() {
    std::string typedName = nameBuffer;
    if (typedName.empty()) return;

    fs::path folder = GetCurrentFolderPath();
    fs::path target = isCreatingFolder ? (folder / typedName) : (folder / (typedName + pendingExtension));
    if (fs::exists(target)) return;

    bool success = false;
    if (isCreatingFolder) {
        success = fs::create_directory(target);
    } else {
        const AssetTypeInfo* info = AssetTypeRegistry::Get().FindByExtension(pendingExtension);
        if (info && info->createAsset) success = info->createAsset(target.string(), typedName);
    }

    if (success) isCreatingNewItem = false;
}

void ContentBrowserPanel::CancelPendingCreate() {
    isCreatingNewItem = false;
}

void ContentBrowserPanel::Render() {
    DrawRectangleRec(bounds, {40, 40, 40, 255});
    DrawRectangleLinesEx(bounds, 1, {80, 80, 80, 255});
    if (detached) {
        DrawRectangleRec(header, {50, 50, 50, 255});
        DrawText(name, (int)bounds.x + 5, (int)bounds.y + 5, 14, WHITE);
    }

    if (Project::Get().GetRootPath().empty()) {
        DrawText("No Project Loaded", (int)bounds.x + 10, (int)bounds.y + (detached ? 35 : 10), 14, GRAY);
        return;
    }

    float topOffset = detached ? 25 : 0;
    Rectangle client = {bounds.x, bounds.y + topOffset, bounds.width, bounds.height - topOffset};
    Vector2 mousePos = GetMousePosition();

    std::string pathLabel = "Content" + (currentRelativeFolder.empty() ? "" : "/" + currentRelativeFolder);
    DrawText(pathLabel.c_str(), (int)client.x + 6, (int)client.y + 4, 12, LIGHTGRAY);

    Rectangle listArea = {client.x, client.y + 22, client.width, client.height - 22};
    float rowHeight = 22;
    float y = listArea.y;

    if (!currentRelativeFolder.empty()) {
        Rectangle row = {listArea.x, y, listArea.width, rowHeight};
        if (GuiButton(row, "..")) NavigateUp();
        y += rowHeight;
    }

    std::vector<ContentEntry> entries = ScanCurrentFolder();
    for (size_t i = 0; i < entries.size(); i++) {
        Rectangle row = {listArea.x, y, listArea.width, rowHeight};
        Color rowColor = (selectedIndex == (int)i) ? Color{70, 70, 95, 255} : Color{45, 45, 45, 255};
        DrawRectangleRec(row, rowColor);

        Color tagColor = entries[i].isFolder ? Color{210, 180, 90, 255} : Color{90, 150, 210, 255};
        DrawRectangleRec({row.x + 4, row.y + 4, 14, 14}, tagColor);
        DrawText(entries[i].displayName.c_str(), (int)row.x + 24, (int)row.y + 4, 14, WHITE);

        if (CheckCollisionPointRec(mousePos, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float currentTime = (float)GetTime();
            if (entries[i].isFolder && lastEntryClickIndex == (int)i && (currentTime - lastEntryClickTime) < 0.3f) {
                NavigateInto(entries[i].displayName);
            } else {
                selectedIndex = (int)i;
            }
            lastEntryClickTime = currentTime;
            lastEntryClickIndex = (int)i;
        }

        y += rowHeight;
    }

    if (isCreatingNewItem) {
        Rectangle row = {listArea.x, y, listArea.width, rowHeight};
        Color tagColor = isCreatingFolder ? Color{210, 180, 90, 255} : Color{90, 150, 210, 255};
        DrawRectangleRec({row.x + 4, row.y + 4, 14, 14}, tagColor);
        GuiTextBox({row.x + 24, row.y, row.width - 28, rowHeight}, nameBuffer, sizeof(nameBuffer), true);

        if (IsKeyPressed(KEY_ENTER)) ConfirmPendingCreate();
        if (IsKeyPressed(KEY_ESCAPE)) CancelPendingCreate();
        y += rowHeight;
    }

    if (!contextMenuOpen && !isCreatingNewItem && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, listArea)) {
        contextMenuOpen = true;
        contextMenuPos = mousePos;
    }

    if (contextMenuOpen) {
        const std::vector<AssetTypeInfo>& assetTypes = AssetTypeRegistry::Get().GetAll();
        float menuHeight = 30.0f * (1 + assetTypes.size());
        Rectangle popup = {contextMenuPos.x, contextMenuPos.y, 150, menuHeight};

        DrawRectangleRec(popup, {60, 60, 60, 255});
        DrawRectangleLinesEx(popup, 1, {90, 90, 90, 255});

        Rectangle folderBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 24};
        if (GuiButton(folderBtn, "New Folder")) BeginCreateFolder();

        for (size_t i = 0; i < assetTypes.size(); i++) {
            Rectangle btn = {popup.x + 4, popup.y + 4 + (float)(i + 1) * 30, popup.width - 8, 24};
            std::string label = "New " + assetTypes[i].typeName;
            if (GuiButton(btn, label.c_str())) BeginCreateAsset(assetTypes[i]);
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            contextMenuOpen = false;
        }
    }
}
