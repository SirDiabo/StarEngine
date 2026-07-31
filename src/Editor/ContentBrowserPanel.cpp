#include "ContentBrowserPanel.h"
#include "Core/Project.h"
#include "Core/StarMaterial.h"
#include "Core/RenderSettings.h"
#include "Core/Module.h"
#include "Core/EmbeddedAssets.h"
#include "Core/OSTrash.h"
#include "Core/SafeProcess.h"
#include "NativeFileDialog.h"
#include "ImportDetailsPanel.h"
#include "AssetImporter.h"
#include "PropertyFields.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace fs = std::filesystem;

void ContentBrowserPanel::LoadIcons() {
    auto load = [&](const std::string& key, const std::string& file) {
        Texture2D tex = EmbeddedAssets::Get().LoadTextureAsset(file);
        if (tex.id != 0) icons[key] = tex;
    };

    load("folder",   "icon_folder.png");
    load("actor",    "icon_actor.png");
    load("starmap",  "icon_starmap.png");
    load("asset",    "icon_asset.png");
    load("texture",  "icon_texture.png");
    load("material", "icon_material.png");
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
    else if (entry.extension == ".startex") key = "texture";
    else if (entry.extension == ".starmat") key = "material";

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
            if (item.fullDiskName == "_Source") continue;
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

void ContentBrowserPanel::OpenEntry(const ContentEntry& entry) {
    if (entry.isFolder) {
        NavigateInto(entry.displayName);
    } else if (entry.extension == ".starmat") {
        fs::path fullPath = fs::path(GetCurrentFolderPath()) / entry.fullDiskName;
        OpenMaterialEditor(fullPath.string());
    } else if (onAssetOpened) {
        fs::path fullPath = fs::path(GetCurrentFolderPath()) / entry.fullDiskName;
        onAssetOpened(fullPath.string(), entry.extension);
    }
}

void ContentBrowserPanel::BeginRename(const ContentEntry& entry) {
    isRenamingItem = true;
    renamingOriginalDiskName = entry.fullDiskName;
    renamingExtension = entry.extension;
    renamingIsFolder = entry.isFolder;
    std::string stem = entry.isFolder ? entry.fullDiskName : fs::path(entry.fullDiskName).stem().string();
    snprintf(nameBuffer, sizeof(nameBuffer), "%s", stem.c_str());
    PropertyField::ForceActivate(nameBuffer, sizeof(nameBuffer));
    itemContextMenuOpen = false;
}

void ContentBrowserPanel::ConfirmRename() {
    std::string typedName = nameBuffer;
    if (typedName.empty()) return;

    fs::path folder = GetCurrentFolderPath();
    fs::path oldPath = folder / renamingOriginalDiskName;
    fs::path newPath = renamingIsFolder ? (folder / typedName) : (folder / (typedName + renamingExtension));

    bool success = true;
    if (newPath != oldPath) {
        std::error_code ec;
        if (fs::exists(newPath, ec)) {
            importStatus = "Rename failed: " + newPath.filename().string() + " already exists";
            success = false;
        } else {
            fs::rename(oldPath, newPath, ec);
            if (ec) {
                importStatus = "Rename failed: " + ec.message();
                success = false;
            }
        }
    }
    if (success) isRenamingItem = false;
}

void ContentBrowserPanel::CancelRename() {
    isRenamingItem = false;
}

void ContentBrowserPanel::DuplicateEntry(const std::string& fullPath, bool isFolder) {
    fs::path src(fullPath);
    fs::path folder = src.parent_path();
    std::string stem = isFolder ? src.filename().string() : src.stem().string();
    std::string ext = isFolder ? "" : src.extension().string();

    fs::path dest;
    int suffix = 1;
    std::error_code ec;
    do {
        std::string candidate = stem + " (" + std::to_string(suffix) + ")" + ext;
        dest = folder / candidate;
        suffix++;
    } while (fs::exists(dest, ec));

    if (isFolder) {
        fs::copy(src, dest, fs::copy_options::recursive, ec);
    } else {
        fs::copy_file(src, dest, ec);
    }
    importStatus = ec ? ("Duplicate failed: " + ec.message()) : ("Duplicated " + dest.stem().string());
}

void ContentBrowserPanel::CopyToClipboard(const std::string& fullPath, bool isFolder) {
    clipboardPath = fullPath;
    clipboardIsFolder = isFolder;
    importStatus = "Copied " + fs::path(fullPath).filename().string();
}

void ContentBrowserPanel::PasteFromClipboard() {
    if (clipboardPath.empty()) return;
    std::error_code existsEc;
    if (!fs::exists(clipboardPath, existsEc)) {
        importStatus = "Paste failed: source no longer exists";
        clipboardPath.clear();
        return;
    }

    fs::path src(clipboardPath);
    fs::path folder = GetCurrentFolderPath();
    std::string stem = clipboardIsFolder ? src.filename().string() : src.stem().string();
    std::string ext = clipboardIsFolder ? "" : src.extension().string();

    fs::path dest = folder / (stem + ext);
    std::error_code ec;
    int suffix = 1;
    while (fs::exists(dest, ec)) {
        std::string candidate = stem + " (" + std::to_string(suffix) + ")" + ext;
        dest = folder / candidate;
        suffix++;
    }

    if (clipboardIsFolder) {
        fs::copy(src, dest, fs::copy_options::recursive, ec);
    } else {
        fs::copy_file(src, dest, ec);
    }
    importStatus = ec ? ("Paste failed: " + ec.message()) : ("Pasted " + dest.stem().string());
}

void ContentBrowserPanel::RequestDelete(const std::string& fullPath, bool isFolder, const std::string& displayName) {
    pendingDeletePath = fullPath;
    pendingDeleteIsFolder = isFolder;
    std::string kind = isFolder ? "folder" : "item";
    deleteConfirm.Open("Move " + kind + " \"" + displayName + "\" to the trash?", "Delete");
    itemContextMenuOpen = false;
}

void ContentBrowserPanel::RevealInExplorer(const std::string& fullPath, bool isFolder) {
    fs::path target = isFolder ? fs::path(fullPath) : fs::path(fullPath).parent_path();
    std::string log;
    std::vector<std::string> argv;
#ifdef _WIN32
    argv = {"explorer", target.string()};
#elif defined(__APPLE__)
    argv = {"open", target.string()};
#else
    argv = {"xdg-open", target.string()};
#endif
    SafeProcess::Run(argv, log);
}

void ContentBrowserPanel::BeginCreateFolder() {
    isCreatingNewItem = true;
    isCreatingFolder = true;
    pendingExtension.clear();
    snprintf(nameBuffer, sizeof(nameBuffer), "NewFolder");
    PropertyField::ForceActivate(nameBuffer, sizeof(nameBuffer));
    contextMenuOpen = false;
}

void ContentBrowserPanel::BeginCreateAsset(const AssetTypeInfo& info) {
    isCreatingNewItem = true;
    isCreatingFolder = false;
    pendingExtension = info.extension;
    snprintf(nameBuffer, sizeof(nameBuffer), "New%s", info.typeName.c_str());
    PropertyField::ForceActivate(nameBuffer, sizeof(nameBuffer));
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
    std::string result = AssetImporter::Cook(srcPath, assetType, extension, GetCurrentFolderPath());
    if (!result.empty()) {
        importStatus = "Imported " + fs::path(result).stem().string();
    } else {
        importStatus = "Import failed: could not write " + fs::path(srcPath).stem().string();
    }
}

void ContentBrowserPanel::ImportPathAuto(const std::string& path) {
    static const std::vector<std::string> meshExts  = {".obj", ".gltf", ".glb", ".iqm"};
    static const std::vector<std::string> texExts   = {".png", ".jpg", ".jpeg", ".bmp"};
    static const std::vector<std::string> soundExts = {".wav"};

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (std::find(meshExts.begin(), meshExts.end(), ext) != meshExts.end()) {
        CookImportedAsset(path, "StaticMesh", ".starasset");
    } else if (std::find(texExts.begin(), texExts.end(), ext) != texExts.end()) {
        CookImportedAsset(path, "Texture", ".startex");
    } else if (std::find(soundExts.begin(), soundExts.end(), ext) != soundExts.end()) {
        CookImportedAsset(path, "Sound", ".starsound");
    } else {
        importStatus = "Import failed: unrecognized file type " + ext;
    }
}

void ContentBrowserPanel::ImportAssetAuto() {
    std::string path = NativeFileDialog::OpenFile(
        "Import Asset", "",
        "Mesh, Texture or Sound (obj, gltf, glb, iqm, png, jpg, bmp, wav)",
        "*.obj;*.gltf;*.glb;*.iqm;*.png;*.jpg;*.jpeg;*.bmp;*.wav");
    if (path.empty()) return;
    ImportPathAuto(path);
}

void ContentBrowserPanel::HandleFileDrop() {
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    for (unsigned int i = 0; i < dropped.count; i++) {
        ImportPathAuto(dropped.paths[i]);
    }
    UnloadDroppedFiles(dropped);
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
    Rectangle box = {bounds.x + bounds.width * 0.5f - 110, bounds.y + bounds.height * 0.5f - 111, 220, 222};
    const UIPalette& theme = UITheme::Get();

    DrawRectangleRec({bounds.x, bounds.y, bounds.width, bounds.height}, {0, 0, 0, 120});
    UIWidget::Panel(box);

    std::string title = fs::path(materialEditorPath).stem().string();
    DrawText(title.c_str(), (int)box.x + 8, (int)box.y + 6, 13, theme.textPrimary);

    float y = box.y + 28;
    DrawText("Base Color", (int)box.x + 8, (int)y, 11, theme.textSecondary);
    y += 16;

    const char* channelLabels[4] = {"R", "G", "B", "A"};
    unsigned char* channels[4] = {&materialEditorData.baseColor.r, &materialEditorData.baseColor.g,
                                   &materialEditorData.baseColor.b, &materialEditorData.baseColor.a};
    float chW = (box.width - 16 - 3 * 6) / 4;
    for (int i = 0; i < 4; i++) {
        float cx = box.x + 8 + i * (chW + 6);
        DrawText(channelLabels[i], (int)cx, (int)y, 10, theme.textSecondary);
        Rectangle r = {cx, y + 12, chW, 18};
        int channelVal = *channels[i];
        if (PropertyField::IntBox(r, materialColorBuf[i], 8, i + 1, materialActiveField, channelVal)) {
            *channels[i] = (unsigned char)(channelVal < 0 ? 0 : (channelVal > 255 ? 255 : channelVal));
        }
    }
    y += 38;

    DrawRectangleRec({box.x + box.width - 34, y - 34, 24, 24}, materialEditorData.baseColor);
    DrawRectangleLinesEx({box.x + box.width - 34, y - 34, 24, 24}, 1, theme.fieldBorder);

    bool shadingAllowsLit = RenderSettings::Get().GetShadingType() != ShadingType::Unlit;
    Color litLabelColor = shadingAllowsLit ? theme.textSecondary : theme.buttonDisabledText;
    DrawText("Lit", (int)box.x + 8, (int)y, 11, litLabelColor);
    GuiSetState(shadingAllowsLit ? STATE_NORMAL : STATE_DISABLED);
    PropertyField::BoolBox({box.x + box.width - 42, y - 2, 18, 18}, materialEditorData.lit);
    GuiSetState(STATE_NORMAL);
    y += 22;

    DrawText("Texture", (int)box.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle texRow = {box.x + 8, y, box.width - 16, 18};
    DrawRectangleRec(texRow, theme.fieldBg);
    DrawRectangleLinesEx(texRow, 1, theme.fieldBorder);
    std::string texLabel = materialEditorData.textureAssetPath.empty()
                                ? "None"
                                : fs::path(materialEditorData.textureAssetPath).stem().string();
    DrawText(texLabel.c_str(), (int)texRow.x + 4, (int)texRow.y + 3, 11, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, texRow) && !pickerBlocksInput) {
        materialTexturePicker.Open({texRow.x, texRow.y + texRow.height}, ".startex");
    }
    y += 26;
    std::string pickedTex;
    if (materialTexturePicker.Render(pickedTex)) materialEditorData.textureAssetPath = pickedTex;

    Rectangle saveBtn = {box.x + 8, box.y + box.height - 28, (box.width - 20) / 2, 22};
    Rectangle closeBtn = {saveBtn.x + saveBtn.width + 4, saveBtn.y, saveBtn.width, 22};
    if (UIWidget::Button(saveBtn, "Save") && !pickerBlocksInput) {
        materialEditorData.SaveToFile(materialEditorPath);
        materialEditorOpen = false;
    }
    if (UIWidget::Button(closeBtn, "Cancel") && !pickerBlocksInput) {
        materialEditorOpen = false;
    }
}

void ContentBrowserPanel::Render() {
    if (!iconsLoaded && IsWindowReady()) LoadIcons();

    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    if (Project::Get().GetRootPath().empty()) {
        DrawText("No Project Loaded", (int)bounds.x + 10, (int)bounds.y + 10, 14, theme.textSecondary);
        return;
    }

    Rectangle client = { bounds.x, bounds.y, bounds.width, bounds.height };
    Vector2 mousePos = GetMousePosition();
    const bool importEnabled = ModuleManager::Get().IsModuleEnabled("Meshes") ||
                                ModuleManager::Get().IsModuleEnabled("Stargraph");
    const bool gridInputBlocked = contextMenuOpen || materialEditorOpen || isCreatingNewItem ||
                                   itemContextMenuOpen || isRenamingItem || deleteConfirm.IsOpen();

    if (importEnabled && !gridInputBlocked && CheckCollisionPointRec(mousePos, client)) {
        HandleFileDrop();
    }

    // Breadcrumb path bar
    Rectangle pathBar = {client.x, client.y, client.width, 22};
    DrawRectangleRec(pathBar, theme.tabBar);

    float pathLabelX = pathBar.x + 6;
    if (importEnabled) {
        Rectangle importBtn = {pathBar.x + 2, pathBar.y + 2, 18, 18};
        if (UIWidget::Button(importBtn, "+") && !gridInputBlocked) ImportAssetAuto();
        pathLabelX = importBtn.x + importBtn.width + 6;
    }

    std::string pathLabel = "Content" + (currentRelativeFolder.empty() ? "" : "/" + currentRelativeFolder);
    DrawText(pathLabel.c_str(), (int)pathLabelX, (int)pathBar.y + 4, 12, theme.textPrimary);

    if (!currentRelativeFolder.empty()) {
        int backW = MeasureText("< Back", 12) + 12;
        Rectangle backBtn = {client.x + client.width - backW - 4, client.y + 2, (float)backW, 18};
        if (UIWidget::Button(backBtn, "< Back") && !gridInputBlocked) NavigateUp();
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

    int dropTargetFolderIndex = -1;
    bool rightClickConsumedByEntry = false;

    for (size_t i = 0; i < entries.size(); i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        float tx = startX + col * cellSize;
        float ty = startY + row * cellSize;

        Rectangle tileRect = {tx, ty, tileSize, tileSize};
        Rectangle iconRect  = {tx + 4, ty + 4, tileSize - 8, tileSize - 22};
        Rectangle labelRect = {tx, ty + tileSize - 18, tileSize, 18};

        bool tileHovered = CheckCollisionPointRec(mousePos, tileRect);
        if (draggingAsset && entries[i].isFolder && tileHovered) {
            dropTargetFolderIndex = (int)i;
        }

        bool isSelected = (selectedIndex == (int)i);
        Color tileBg = UIWidget::SelectableBg(tileHovered, isSelected, theme.buttonBg);
        if (draggingAsset && (int)i == dropTargetFolderIndex)
            tileBg = {70, 110, 80, 255};

        DrawRectangleRounded(tileRect, 0.1f, 4, tileBg);
        if (isSelected) DrawRectangleRoundedLinesEx(tileRect, 0.1f, 4, 1.5f, theme.accent);
        if (draggingAsset && (int)i == dropTargetFolderIndex)
            DrawRectangleRoundedLinesEx(tileRect, 0.1f, 4, 1.5f, {110, 200, 130, 255});

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

        bool renamingThis = isRenamingItem && entries[i].fullDiskName == renamingOriginalDiskName;

        if (!renamingThis) {
            std::string label = entries[i].displayName;
            int labelW = MeasureText(label.c_str(), 10);
            if (labelW > (int)tileSize - 6) label = label.substr(0, (int)(tileSize / 7)) + "..";
            int lx = (int)(tx + tileSize / 2 - MeasureText(label.c_str(), 10) / 2);
            DrawText(label.c_str(), lx, (int)labelRect.y + 3, 10, theme.textPrimary);
        } else {
            GuiTextBox({labelRect.x + 2, labelRect.y, labelRect.width - 4, labelRect.height},
                       nameBuffer, sizeof(nameBuffer), true);
            if (IsKeyPressed(KEY_ENTER)) ConfirmRename();
            if (IsKeyPressed(KEY_ESCAPE)) CancelRename();
        }

        if (!gridInputBlocked && tileHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float currentTime = (float)GetTime();
            if (lastEntryClickIndex == (int)i && (currentTime - lastEntryClickTime) < 0.3f) {
                OpenEntry(entries[i]);
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

        if (!gridInputBlocked && tileHovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            selectedIndex = (int)i;
            fs::path fullPath = fs::path(GetCurrentFolderPath()) / entries[i].fullDiskName;
            contextTargetPath = fullPath.string();
            contextTargetDisplayName = entries[i].displayName;
            contextTargetExtension = entries[i].extension;
            contextTargetIsFolder = entries[i].isFolder;
            itemContextMenuOpen = true;
            itemContextMenuJustOpened = true;
            itemContextMenuPos = mousePos;
            rightClickConsumedByEntry = true;
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
        DrawRectangleRoundedLinesEx(tileRect, 0.1f, 4, 1.5f, theme.accent);
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
            if (dropTargetFolderIndex != -1) {
                fs::path destFolder = fs::path(GetCurrentFolderPath()) / entries[dropTargetFolderIndex].fullDiskName;
                fs::path destPath = destFolder / fs::path(dragEntryPath).filename();
                std::error_code ec;
                if (fs::exists(destPath, ec)) {
                    importStatus = "Move failed: " + destPath.filename().string() + " already exists there";
                } else {
                    fs::rename(dragEntryPath, destPath, ec);
                    importStatus = !ec ? "Moved " + destPath.stem().string()
                                        : "Move failed: " + ec.message();
                }
            } else if (onAssetDragged) {
                onAssetDragged(dragEntryPath, dragEntryExt, mousePos);
            }
            draggingAsset = false;
            dragEntryIndex = -1;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !draggingAsset) dragEntryIndex = -1;

    if (!contextMenuOpen && !isCreatingNewItem && !materialEditorOpen && !itemContextMenuOpen &&
        !isRenamingItem && !rightClickConsumedByEntry &&
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
            UIWidget::Panel(popup);

            Rectangle folderBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 24};
            if (UIWidget::Button(folderBtn, "New Folder")) BeginCreateFolder();

            for (size_t i = 0; i < creatableTypes.size(); i++) {
                Rectangle btn = {popup.x + 4, popup.y + 4 + (float)(i + 1) * 30, popup.width - 8, 24};
                std::string label = "New " + creatableTypes[i]->typeName;
                if (UIWidget::Button(btn, label.c_str())) BeginCreateAsset(*creatableTypes[i]);
            }
        }
    }

    if (!gridInputBlocked && selectedIndex >= 0 && selectedIndex < (int)entries.size() &&
        IsKeyPressed(KEY_DELETE)) {
        const ContentEntry& entry = entries[selectedIndex];
        fs::path fullPath = fs::path(GetCurrentFolderPath()) / entry.fullDiskName;
        RequestDelete(fullPath.string(), entry.isFolder, entry.displayName);
    }

    if (!gridInputBlocked && selectedIndex >= 0 && selectedIndex < (int)entries.size() &&
        IsKeyPressed(KEY_F2)) {
        BeginRename(entries[selectedIndex]);
    }

    if (itemContextMenuOpen) {
        bool showReimport = !contextTargetIsFolder &&
            (contextTargetExtension == ".starasset" || contextTargetExtension == ".startex" ||
             contextTargetExtension == ".starsound");
        bool showPaste = !clipboardPath.empty();

        std::vector<std::string> items = {"Open", "Rename", "Duplicate", "Copy"};
        if (showPaste) items.push_back("Paste");
        items.push_back("Copy Path");
        if (showReimport) items.push_back("Reimport");
        items.push_back("View in Explorer");
        items.push_back("Delete");

        float menuHeight = 26.0f * (float)items.size() + 4.0f;
        float popupX = itemContextMenuPos.x;
        float popupY = itemContextMenuPos.y;
        if (popupX + 150 > bounds.x + bounds.width)  popupX = bounds.x + bounds.width - 150;
        if (popupY + menuHeight > bounds.y + bounds.height) popupY = bounds.y + bounds.height - menuHeight;
        if (popupX < bounds.x) popupX = bounds.x;
        if (popupY < bounds.y) popupY = bounds.y;
        Rectangle popup = {popupX, popupY, 150, menuHeight};

        if (itemContextMenuJustOpened) {
            itemContextMenuJustOpened = false;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            itemContextMenuOpen = false;
        }

        if (itemContextMenuOpen) {
            UIWidget::Panel(popup);

            for (size_t i = 0; i < items.size(); i++) {
                Rectangle btn = {popup.x + 4, popup.y + 4 + (float)i * 26, popup.width - 8, 22};
                if (!UIWidget::Button(btn, items[i].c_str())) continue;

                if (items[i] == "Open") {
                    ContentEntry target;
                    target.displayName = contextTargetDisplayName;
                    target.fullDiskName = contextTargetIsFolder ? contextTargetDisplayName
                                                                  : fs::path(contextTargetPath).filename().string();
                    target.isFolder = contextTargetIsFolder;
                    target.extension = contextTargetExtension;
                    OpenEntry(target);
                    itemContextMenuOpen = false;
                } else if (items[i] == "Rename") {
                    ContentEntry target;
                    target.displayName = contextTargetDisplayName;
                    target.fullDiskName = contextTargetIsFolder ? contextTargetDisplayName
                                                                  : fs::path(contextTargetPath).filename().string();
                    target.isFolder = contextTargetIsFolder;
                    target.extension = contextTargetExtension;
                    BeginRename(target);
                } else if (items[i] == "Duplicate") {
                    DuplicateEntry(contextTargetPath, contextTargetIsFolder);
                    itemContextMenuOpen = false;
                } else if (items[i] == "Copy") {
                    CopyToClipboard(contextTargetPath, contextTargetIsFolder);
                    itemContextMenuOpen = false;
                } else if (items[i] == "Paste") {
                    PasteFromClipboard();
                    itemContextMenuOpen = false;
                } else if (items[i] == "Copy Path") {
                    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
                    std::error_code ec;
                    fs::path rel = fs::relative(contextTargetPath, contentRoot, ec);
                    std::string virtualPath = "Content/" + (ec ? contextTargetPath : rel.generic_string());
                    SetClipboardText(virtualPath.c_str());
                    importStatus = "Copied path to clipboard";
                    itemContextMenuOpen = false;
                } else if (items[i] == "Reimport") {
                    std::string result = AssetImporter::Reimport(contextTargetPath);
                    importStatus = result.empty() ? "Reimport failed" : "Reimported " + contextTargetDisplayName;
                    itemContextMenuOpen = false;
                } else if (items[i] == "View in Explorer") {
                    RevealInExplorer(contextTargetPath, contextTargetIsFolder);
                    itemContextMenuOpen = false;
                } else if (items[i] == "Delete") {
                    RequestDelete(contextTargetPath, contextTargetIsFolder, contextTargetDisplayName);
                }
                break;
            }
        }
    }

    bool deleteConfirmed = false;
    if (deleteConfirm.Render(deleteConfirmed)) {
        if (deleteConfirmed) {
            std::string trashError;
            if (OSTrash::MoveToTrash(pendingDeletePath, trashError)) {
                importStatus = pendingDeleteIsFolder ? "Moved folder to trash" : "Moved to trash";
                if (selectedIndex >= 0 && selectedIndex < (int)entries.size() &&
                    fs::path(GetCurrentFolderPath()) / entries[selectedIndex].fullDiskName == fs::path(pendingDeletePath)) {
                    selectedIndex = -1;
                }
            } else {
                importStatus = "Delete failed: " + trashError;
            }
        }
        pendingDeletePath.clear();
    }


    if (!importStatus.empty()) {
        DrawText(importStatus.c_str(), (int)client.x + 8, (int)(client.y + client.height - 16), 11,
                  theme.success);
    }

    RenderMaterialEditor();
}