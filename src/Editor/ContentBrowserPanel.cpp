#include "ContentBrowserPanel.h"
#include "Core/Project.h"
#include "Core/AssetRegistry.h"
#include "Core/StarAsset.h"
#include "Core/Module.h"
#include "NativeFileDialog.h"
#include "ImportDetailsPanel.h"
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

void ContentBrowserPanel::CookImportedAsset(const std::string& srcPath, const std::string& assetType,
                                             const std::string& extension) {
    fs::path src(srcPath);
    std::error_code ec;
    fs::path absoluteSrc = fs::absolute(src, ec);
    if (ec) absoluteSrc = src;

    std::string assetName = src.stem().string();
    fs::path folder = GetCurrentFolderPath();
    fs::path target = folder / (assetName + extension);
    int suffix = 1;
    while (fs::exists(target)) {
        target = folder / (assetName + "_" + std::to_string(suffix++) + extension);
    }

    StarAsset asset;
    asset.assetType = assetType;
    asset.sourcePath = absoluteSrc.string();
    if (assetType == "StaticMesh") {
        asset.importScaleX = asset.importScaleY = asset.importScaleZ = 100.0f;
    }

    if (asset.SaveToFile(target.string())) {
        AssetRegistry::Get().RegisterAsset(target.string(), asset.sourcePath, target.string());
        importStatus = "Imported " + target.stem().string();
        ImportDetailsPanel::Get().Open(target.string(), assetType);
    } else {
        importStatus = "Import failed: could not write " + target.string();
    }
}

void ContentBrowserPanel::ImportAssetAuto() {
    static const std::vector<std::string> meshExts = {".obj", ".gltf", ".glb", ".iqm"};
    static const std::vector<std::string> texExts   = {".png", ".jpg", ".jpeg", ".bmp"};

    std::string path = NativeFileDialog::OpenFile(
        "Import Asset", "",
        "Mesh or Texture (obj, gltf, glb, iqm, png, jpg, bmp)",
        "*.obj;*.gltf;*.glb;*.iqm;*.png;*.jpg;*.jpeg;*.bmp");
    if (path.empty()) return;

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (std::find(meshExts.begin(), meshExts.end(), ext) != meshExts.end()) {
        CookImportedAsset(path, "StaticMesh", ".starasset");
    } else if (std::find(texExts.begin(), texExts.end(), ext) != texExts.end()) {
        CookImportedAsset(path, "Texture", ".startex");
    } else {
        importStatus = "Import failed: unrecognized file type " + ext;
    }
}

void ContentBrowserPanel::OpenMaterialEditor(const std::string& fullPath) {
    materialEditorPath = fullPath;
    materialEditorData = StarMaterial{};
    StarMaterial::LoadFromFile(fullPath, materialEditorData);

    snprintf(materialColorBuf[0], 8, "%d", materialEditorData.baseColor.r);
    snprintf(materialColorBuf[1], 8, "%d", materialEditorData.baseColor.g);
    snprintf(materialColorBuf[2], 8, "%d", materialEditorData.baseColor.b);
    snprintf(materialColorBuf[3], 8, "%d", materialEditorData.baseColor.a);
    materialActiveField = 0;
    materialTexturePicker.Close();
    materialEditorOpen = true;
}

void ContentBrowserPanel::RenderMaterialEditor() {
    if (!materialEditorOpen) return;

    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = materialTexturePicker.ContainsPoint(mp);
    Rectangle box = {bounds.x + bounds.width * 0.5f - 110, bounds.y + bounds.height * 0.5f - 100, 220, 200};

    DrawRectangleRec({bounds.x, bounds.y, bounds.width, bounds.height}, {0, 0, 0, 120});
    DrawRectangleRec(box, {45, 45, 50, 255});
    DrawRectangleLinesEx(box, 1, {90, 90, 98, 255});

    std::string title = fs::path(materialEditorPath).stem().string();
    DrawText(title.c_str(), (int)box.x + 8, (int)box.y + 6, 13, WHITE);

    float y = box.y + 28;
    DrawText("Base Color", (int)box.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 16;

    const char* channelLabels[4] = {"R", "G", "B", "A"};
    unsigned char* channels[4] = {&materialEditorData.baseColor.r, &materialEditorData.baseColor.g,
                                   &materialEditorData.baseColor.b, &materialEditorData.baseColor.a};
    float chW = (box.width - 16 - 3 * 6) / 4;
    for (int i = 0; i < 4; i++) {
        float cx = box.x + 8 + i * (chW + 6);
        DrawText(channelLabels[i], (int)cx, (int)y, 10, {140, 140, 150, 255});
        Rectangle r = {cx, y + 12, chW, 18};
        if (materialActiveField == i + 1) {
            GuiTextBox(r, materialColorBuf[i], 8, true);
            if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, r))) {
                int v = atoi(materialColorBuf[i]);
                *channels[i] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
                materialActiveField = 0;
            }
        } else {
            bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r);
            if (clicked) GuiTextBox(r, materialColorBuf[i], 8, false);
            DrawRectangleRec(r, {30, 30, 32, 255});
            DrawRectangleLinesEx(r, 1, {65, 65, 75, 255});
            DrawText(materialColorBuf[i], (int)r.x + 4, (int)r.y + 3, 11, WHITE);
            if (clicked) materialActiveField = i + 1;
        }
    }
    y += 38;

    DrawRectangleRec({box.x + box.width - 34, y - 34, 24, 24}, materialEditorData.baseColor);
    DrawRectangleLinesEx({box.x + box.width - 34, y - 34, 24, 24}, 1, {90, 90, 98, 255});

    DrawText("Texture", (int)box.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;
    Rectangle texRow = {box.x + 8, y, box.width - 16, 18};
    DrawRectangleRec(texRow, {30, 30, 32, 255});
    DrawRectangleLinesEx(texRow, 1, {65, 65, 75, 255});
    std::string texLabel = materialEditorData.textureAssetPath.empty()
                                ? "None"
                                : fs::path(materialEditorData.textureAssetPath).stem().string();
    DrawText(texLabel.c_str(), (int)texRow.x + 4, (int)texRow.y + 3, 11, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, texRow) && !pickerBlocksInput) {
        materialTexturePicker.Open({texRow.x, texRow.y + texRow.height}, ".startex");
    }
    y += 26;
    std::string pickedTex;
    if (materialTexturePicker.Render(pickedTex)) materialEditorData.textureAssetPath = pickedTex;

    Rectangle saveBtn = {box.x + 8, box.y + box.height - 28, (box.width - 20) / 2, 22};
    Rectangle closeBtn = {saveBtn.x + saveBtn.width + 4, saveBtn.y, saveBtn.width, 22};
    if (GuiButton(saveBtn, "Save") && !pickerBlocksInput) {
        materialEditorData.SaveToFile(materialEditorPath);
        materialEditorOpen = false;
    }
    if (GuiButton(closeBtn, "Cancel") && !pickerBlocksInput) {
        materialEditorOpen = false;
    }
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
    const bool meshImportEnabled = ModuleManager::Get().IsModuleEnabled("Meshes");

    // Breadcrumb path bar
    Rectangle pathBar = {client.x, client.y, client.width, 22};
    DrawRectangleRec(pathBar, {30, 30, 34, 255});

    float pathLabelX = pathBar.x + 6;
    if (meshImportEnabled) {
        Rectangle importBtn = {pathBar.x + 2, pathBar.y + 2, 18, 18};
        if (GuiButton(importBtn, "+")) ImportAssetAuto();
        pathLabelX = importBtn.x + importBtn.width + 6;
    }

    std::string pathLabel = "Content" + (currentRelativeFolder.empty() ? "" : "/" + currentRelativeFolder);
    DrawText(pathLabel.c_str(), (int)pathLabelX, (int)pathBar.y + 4, 12, LIGHTGRAY);

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
                } else if (entries[i].extension == ".starmat") {
                    fs::path fullPath = fs::path(GetCurrentFolderPath()) / entries[i].fullDiskName;
                    OpenMaterialEditor(fullPath.string());
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
        const std::vector<AssetTypeInfo>& allAssetTypes = AssetTypeRegistry::Get().GetFilteredTypes();
        std::vector<const AssetTypeInfo*> creatableTypes;
        for (const AssetTypeInfo& info : allAssetTypes) {
            if (info.createAsset) creatableTypes.push_back(&info);
        }

        float menuHeight = 30.0f * (1 + creatableTypes.size());
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

            for (size_t i = 0; i < creatableTypes.size(); i++) {
                Rectangle btn = {popup.x + 4, popup.y + 4 + (float)(i + 1) * 30, popup.width - 8, 24};
                std::string label = "New " + creatableTypes[i]->typeName;
                if (GuiButton(btn, label.c_str())) BeginCreateAsset(*creatableTypes[i]);
            }
        }
    }

    // Import status
    if (!importStatus.empty()) {
        DrawText(importStatus.c_str(), (int)client.x + 8, (int)(client.y + client.height - 16), 11,
                  {150, 200, 150, 255});
    }

    RenderMaterialEditor();
}