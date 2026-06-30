#include "ContentBrowserPanel.h"
#include "Core/Project.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace fs = std::filesystem;

static std::string FindAssetsDir() {
    fs::path exe = GetApplicationDirectory();
    for (int i = 0; i < 5; i++) {
        fs::path candidate = exe / "Assets";
        if (fs::exists(candidate)) return candidate.string();
        exe = exe.parent_path();
    }
    return "";
}

void ContentBrowserPanel::LoadIcons() {
    std::string dir = FindAssetsDir();
    if (dir.empty()) return;

    auto load = [&](const std::string& key, const std::string& file) {
        fs::path p = fs::path(dir) / file;
        if (fs::exists(p)) {
            Texture2D tex = LoadTexture(p.string().c_str());
            if (tex.id != 0) icons[key] = tex;
        }
    };

    load("folder",   "icon_folder.png");
    load("actor",    "icon_actor.png");
    load("starmap",  "icon_starmap.png");
    load("asset",    "icon_asset.png");
    iconsLoaded = true;
}

void ContentBrowserPanel::UnloadIcons() {
    for (auto& kv : icons) UnloadTexture(kv.second);
    icons.clear();
}

ContentBrowserPanel::~ContentBrowserPanel() {
    UnloadIcons();
}

void ContentBrowserPanel::Initialize() {
    name = "Content Browser";
}

Texture2D ContentBrowserPanel::GetIconForEntry(const ContentEntry& entry) const {
    std::string key = "asset";
    if (entry.isFolder) key = "folder";
    else if (entry.extension == ".staractor" || entry.extension == ".stargraph") key = "actor";
    else if (entry.extension == ".starmap") key = "starmap";

    auto it = icons.find(key);
    if (it != icons.end()) return it->second;
    auto fallback = icons.find("asset");
    if (fallback != icons.end()) return fallback->second;
    return Texture2D{};
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
        item.extension    = entry.path().extension().string();

        if (entry.is_directory()) {
            item.isFolder = true;
            item.displayName = item.fullDiskName;
            folders.push_back(item);
        } else {
            item.isFolder = false;
            item.displayName = entry.path().stem().string();
            const AssetTypeInfo* info = AssetTypeRegistry::Get().FindByExtension(item.extension);
            item.typeName = info ? info->typeName : item.extension;
            files.push_back(item);
        }
    }

    folders.insert(folders.end(), files.begin(), files.end());
    return folders;
}

void ContentBrowserPanel::NavigateInto(const std::string& folderName) {
    currentRelativeFolder = currentRelativeFolder.empty()
        ? folderName
        : currentRelativeFolder + "/" + folderName;
    selectedIndex = -1;
}

void ContentBrowserPanel::NavigateUp() {
    size_t sep = currentRelativeFolder.find_last_of('/');
    currentRelativeFolder = (sep == std::string::npos) ? "" : currentRelativeFolder.substr(0, sep);
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
    fs::path target = isCreatingFolder
        ? (folder / typedName)
        : (folder / (typedName + pendingExtension));

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
    if (!iconsLoaded && IsWindowReady()) LoadIcons();

    DrawRectangleRec(bounds, {38, 38, 42, 255});
    DrawRectangleLinesEx(bounds, 1, {65, 65, 70, 255});

    if (Project::Get().GetRootPath().empty()) {
        DrawText("No Project Loaded", (int)bounds.x + 10, (int)bounds.y + 10, 14, GRAY);
        return;
    }

    Rectangle client = { bounds.x, bounds.y, bounds.width, bounds.height };
    Vector2 mousePos = GetMousePosition();

    // Breadcrumb path bar
    Rectangle pathBar = {client.x, client.y, client.width, 22};
    DrawRectangleRec(pathBar, {30, 30, 34, 255});
    std::string pathLabel = "Content" + (currentRelativeFolder.empty() ? "" : "/" + currentRelativeFolder);
    DrawText(pathLabel.c_str(), (int)pathBar.x + 6, (int)pathBar.y + 4, 12, LIGHTGRAY);

    if (!currentRelativeFolder.empty()) {
        int backW = MeasureText("< Back", 12) + 12;
        Rectangle backBtn = {client.x + client.width - backW - 4, client.y + 2, (float)backW, 18};
        if (GuiButton(backBtn, "< Back")) NavigateUp();
    }

    Rectangle gridArea = {client.x, client.y + 22, client.width, client.height - 22};
    std::vector<ContentEntry> entries = ScanCurrentFolder();

    float padding = 8.0f;
    float cellSize = tileSize + padding;
    int cols = (int)((gridArea.width - padding) / cellSize);
    if (cols < 1) cols = 1;

    float startX = gridArea.x + padding;
    float startY = gridArea.y + padding;

    BeginScissorMode((int)gridArea.x, (int)gridArea.y, (int)gridArea.width, (int)gridArea.height);

    for (size_t i = 0; i < entries.size(); i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        float tx = startX + col * cellSize;
        float ty = startY + row * cellSize;

        Rectangle tileRect = {tx, ty, tileSize, tileSize};
        Rectangle iconRect  = {tx + 4, ty + 4, tileSize - 8, tileSize - 22};
        Rectangle labelRect = {tx, ty + tileSize - 18, tileSize, 18};

        bool isSelected = (selectedIndex == (int)i);
        Color tileBg = isSelected ? Color{60, 80, 120, 255} : Color{48, 48, 54, 255};
        if (CheckCollisionPointRec(mousePos, tileRect) && !isSelected)
            tileBg = {55, 55, 62, 255};

        DrawRectangleRounded(tileRect, 0.1f, 4, tileBg);
        if (isSelected) DrawRectangleRoundedLinesEx(tileRect, 0.1f, 4, 1.5f, {100, 140, 220, 255});

        Texture2D icon = GetIconForEntry(entries[i]);
        if (icon.id != 0) {
            DrawTexturePro(icon,
                {0, 0, (float)icon.width, (float)icon.height},
                {iconRect.x + 4, iconRect.y, iconRect.width - 8, iconRect.height},
                {0, 0}, 0, WHITE);
        } else {
            Color iconColor = entries[i].isFolder ? Color{210, 180, 90, 255} : Color{90, 150, 210, 255};
            DrawRectangleRec({iconRect.x + iconRect.width * 0.25f, iconRect.y + iconRect.height * 0.2f,
                              iconRect.width * 0.5f, iconRect.height * 0.6f}, iconColor);
        }

        DrawRectangleRec(labelRect, {35, 35, 40, 200});
        std::string label = entries[i].displayName;
        int labelW = MeasureText(label.c_str(), 10);
        if (labelW > (int)tileSize - 6) label = label.substr(0, (int)(tileSize / 7)) + "..";
        int lx = (int)(tx + tileSize / 2 - MeasureText(label.c_str(), 10) / 2);
        DrawText(label.c_str(), lx, (int)labelRect.y + 3, 10, WHITE);

        if (CheckCollisionPointRec(mousePos, tileRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float currentTime = (float)GetTime();
            if (lastEntryClickIndex == (int)i && (currentTime - lastEntryClickTime) < 0.3f) {
                if (entries[i].isFolder) {
                    NavigateInto(entries[i].displayName);
                } else if (onAssetOpened) {
                    fs::path fullPath = fs::path(GetCurrentFolderPath()) / entries[i].fullDiskName;
                    onAssetOpened(fullPath.string(), entries[i].extension);
                }
            } else {
                selectedIndex = (int)i;
                if (!entries[i].isFolder) {
                    dragEntryIndex = (int)i;
                    dragPressPos   = mousePos;
                    fs::path fullPath = fs::path(GetCurrentFolderPath()) / entries[i].fullDiskName;
                    dragEntryPath = fullPath.string();
                    dragEntryExt  = entries[i].extension;
                }
            }
            lastEntryClickTime = currentTime;
            lastEntryClickIndex = (int)i;
        }

        if (!entries[i].isFolder && dragEntryIndex == (int)i && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - dragPressPos.x;
            float dy = mousePos.y - dragPressPos.y;
            if (dx*dx + dy*dy > 36) draggingAsset = true;
        }
    }

    if (isCreatingNewItem) {
        int col = (int)entries.size() % cols;
        int row = (int)entries.size() / cols;
        float tx = startX + col * cellSize;
        float ty = startY + row * cellSize;
        Rectangle tileRect = {tx, ty, tileSize, tileSize};
        DrawRectangleRounded(tileRect, 0.1f, 4, {55, 80, 100, 255});
        DrawRectangleRoundedLinesEx(tileRect, 0.1f, 4, 1.5f, {100, 160, 220, 255});
        GuiTextBox({tx + 2, ty + tileSize - 20, tileSize - 4, 18}, nameBuffer, sizeof(nameBuffer), true);
        if (IsKeyPressed(KEY_ENTER)) ConfirmPendingCreate();
        if (IsKeyPressed(KEY_ESCAPE)) CancelPendingCreate();
    }

    EndScissorMode();

    if (draggingAsset) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            std::string label = fs::path(dragEntryPath).stem().string();
            int lw = MeasureText(label.c_str(), 12) + 16;
            DrawRectangleRec({mousePos.x + 10, mousePos.y - 2, (float)lw, 18}, {55, 55, 75, 220});
            DrawRectangleLinesEx({mousePos.x + 10, mousePos.y - 2, (float)lw, 18}, 1, {120, 120, 200, 255});
            DrawRectangleRec({mousePos.x + 14, mousePos.y + 2, 10, 10}, {90, 150, 210, 255});
            DrawText(label.c_str(), (int)mousePos.x + 28, (int)mousePos.y + 1, 12, WHITE);
        } else {
            if (onAssetDragged) onAssetDragged(dragEntryPath, dragEntryExt, mousePos);
            draggingAsset = false;
            dragEntryIndex = -1;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !draggingAsset) dragEntryIndex = -1;

    if (!contextMenuOpen && !isCreatingNewItem &&
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, gridArea)) {
        contextMenuOpen = true;
        contextMenuJustOpened = true;
        contextMenuPos = mousePos;
    }

    if (contextMenuOpen) {
        const std::vector<AssetTypeInfo>& assetTypes = AssetTypeRegistry::Get().GetAll();
        float menuHeight = 30.0f * (1 + assetTypes.size());
        float popupX = contextMenuPos.x;
        float popupY = contextMenuPos.y;
        if (popupX + 150 > bounds.x + bounds.width)  popupX = bounds.x + bounds.width - 150;
        if (popupY + menuHeight > bounds.y + bounds.height) popupY = bounds.y + bounds.height - menuHeight;
        if (popupX < bounds.x) popupX = bounds.x;
        if (popupY < bounds.y) popupY = bounds.y;
        Rectangle popup = {popupX, popupY, 150, menuHeight};

        if (contextMenuJustOpened) {
            contextMenuJustOpened = false;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            contextMenuOpen = false;
        }

        if (contextMenuOpen) {
            DrawRectangleRec(popup, {55, 55, 60, 255});
            DrawRectangleLinesEx(popup, 1, {85, 85, 90, 255});

            Rectangle folderBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 24};
            if (GuiButton(folderBtn, "New Folder")) BeginCreateFolder();

            for (size_t i = 0; i < assetTypes.size(); i++) {
                Rectangle btn = {popup.x + 4, popup.y + 4 + (float)(i + 1) * 30, popup.width - 8, 24};
                std::string label = "New " + assetTypes[i].typeName;
                if (GuiButton(btn, label.c_str())) BeginCreateAsset(assetTypes[i]);
            }
        }
    }
}