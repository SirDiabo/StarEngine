#include "ContentBrowserPanel.h"
#include "ContentUndo.h"
#include "Core/Project.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/RenderSettings.h"
#include "Core/ShadingModel.h"
#include "Core/EmbeddedAssets.h"
#include "Runtime/AssetCache.h"
#include "Core/OSTrash.h"
#include "Core/SafeProcess.h"
#include "Core/SourceFormatConverterRegistry.h"
#include "NativeFileDialog.h"
#include "ImportDetailsPanel.h"
#include "AssetImporter.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/DragDropController.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/HorizontalBoxElement.h"
#include "Core/Widgets/BoxElement.h"
#include "Core/Widgets/GridElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ImageElement.h"
#include "Core/Widgets/RectangleElement.h"
#include "Core/Widgets/CheckBoxElement.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

namespace {

void CollectVendorDirForAsset(const fs::path& assetFile, const fs::path& contentRoot, std::set<fs::path>& outDirs) {
    std::string ext = assetFile.extension().string();
    if (ext != ".starasset" && ext != ".startex" && ext != ".staranim") return;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(assetFile.string(), asset) || asset.vendoredSourcePath.empty()) return;

    fs::path vendorDir = (contentRoot / asset.vendoredSourcePath).parent_path().lexically_normal();
    fs::path vendorRoot = (contentRoot / "_Source").lexically_normal();
    fs::path rel = vendorDir.lexically_relative(vendorRoot);
    if (rel.empty() || rel == "." || *rel.begin() == "..") return;

    outDirs.insert(vendorDir);
}

void TrashVendorDirs(const std::set<fs::path>& dirs) {
    for (const fs::path& dir : dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) continue;
        std::string trashError;
        OSTrash::MoveToTrash(dir.string(), trashError);
    }
}

}

void ContentBrowserPanel::LoadIcons() {
    auto load = [&](const std::string& key, const std::string& file) {
        Texture2D tex = EmbeddedAssets::Get().LoadTextureAsset(file);
        if (tex.id != 0) icons[key] = tex;
    };

    load("folder",      "icon_folder.png");
    load("actor",       "icon_actor.png");
    load("map",         "icon_map.png");
    load("asset",       "icon_asset.png");
    load("texture",     "icon_texture.png");
    load("material",    "icon_material.png");
    load("animation",   "icon_anim.png");
    load("sound",       "icon_sound.png");
    load("widget",      "icon_widget.png");
    iconsLoaded = true;
}

void ContentBrowserPanel::UnloadIcons() {
    for (auto& kv : icons) UnloadTexture(kv.second);
    icons.clear();
}

ContentBrowserPanel::~ContentBrowserPanel() {
    UnloadIcons();
    delete chrome;
    delete materialChrome;
    delete createNameChrome;
}

void ContentBrowserPanel::Initialize() {
    name = "Content Browser";
}

const Texture2D* ContentBrowserPanel::GetIconTexturePtr(const ContentEntry& entry) const {
    std::string key = "asset";
    if (entry.isFolder) key = "folder";
    else if (entry.extension == ".staractor")   key = "actor";
    else if (entry.extension == ".starmap")     key = "map";
    else if (entry.extension == ".startex")     key = "texture";
    else if (entry.extension == ".starmat")     key = "material";
    else if (entry.extension == ".staranim")    key = "animation";
    else if (entry.extension == ".starsound")   key = "sound";
    else if (entry.extension == ".starwidget")  key = "widget";

    auto it = icons.find(key);
    if (it != icons.end() && it->second.id != 0) return &it->second;
    auto fallback = icons.find("asset");
    if (fallback != icons.end() && fallback->second.id != 0) return &fallback->second;
    return nullptr;
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

void ContentBrowserPanel::NavigateToRelativeFolder(const std::string& relativePath) {
    currentRelativeFolder = relativePath;
    if (currentRelativeFolder == ".") currentRelativeFolder.clear();
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
            } else if (!renamingIsFolder) {
                const AssetTypeInfo* info = AssetTypeRegistry::Get().FindByExtension(renamingExtension);
                if (info && info->renameAsset) info->renameAsset(newPath.string(), typedName);
            }
            if (!ec) {
                std::string extension = renamingExtension;
                bool wasFolder = renamingIsFolder;
                ContentUndo::RecordMove(oldPath, newPath, "Rename", [extension, wasFolder](const fs::path& current) {
                    if (wasFolder) return;
                    const AssetTypeInfo* typeInfo = AssetTypeRegistry::Get().FindByExtension(extension);
                    if (typeInfo && typeInfo->renameAsset) typeInfo->renameAsset(current.string(), current.stem().string());
                });
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
}

void ContentBrowserPanel::BeginCreateAsset(const AssetTypeInfo& info) {
    isCreatingNewItem = true;
    isCreatingFolder = false;
    pendingExtension = info.extension;
    snprintf(nameBuffer, sizeof(nameBuffer), "New%s", info.typeName.c_str());
}

void ContentBrowserPanel::ConfirmPendingCreate() {
    std::string typedName = nameBuffer;
    if (typedName.empty()) return;

    fs::path folder = GetCurrentFolderPath();
    fs::path target = isCreatingFolder
        ? (folder / typedName)
        : (folder / (typedName + pendingExtension));

    if (fs::exists(target)) {
        importStatus = "Create failed: " + target.filename().string() + " already exists";
        return;
    }

    bool success = false;
    if (isCreatingFolder) {
        std::error_code ec;
        success = fs::create_directory(target, ec);
    } else {
        const AssetTypeInfo* info = AssetTypeRegistry::Get().FindByExtension(pendingExtension);
        if (info && info->createAsset) success = info->createAsset(target.string(), typedName);
    }

    if (success) isCreatingNewItem = false;
    else importStatus = "Create failed: could not create " + target.filename().string();
}

void ContentBrowserPanel::CancelPendingCreate() {
    isCreatingNewItem = false;
}

void ContentBrowserPanel::CookImportedAsset(const std::string& srcPath, const std::string& assetType,
                                            const std::string& extension) {
    std::string result = AssetImporter::Cook(srcPath, assetType, extension, GetCurrentFolderPath());
    if (!result.empty()) {
        importStatus = "Imported " + fs::path(result).stem().string();
        ImportDetailsPanel::Get().Open(result, assetType);
    } else {
        importStatus = "Import failed: could not write " + fs::path(srcPath).stem().string();
    }
}

void ContentBrowserPanel::BeginMeshImport(const std::string& srcPath) {
    ImportDetailsPanel::Get().OpenForImport(srcPath, "StaticMesh", ".starasset", GetCurrentFolderPath());
}

void ContentBrowserPanel::ImportPathAuto(const std::string& path) {
    static const std::vector<std::string> meshExts  = {".obj", ".gltf", ".glb", ".iqm"};
    static const std::vector<std::string> texExts   = {".png", ".jpg", ".jpeg", ".bmp"};
    static const std::vector<std::string> soundExts = {".wav"};

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string actualPath = path;
    const SourceFormatConverter* converter = SourceFormatConverterRegistry::Get().FindAvailable(ext);
    if (converter) {
        std::string convertedPath, convertError;
        bool converted = converter->convert(path, fs::temp_directory_path().string(), convertedPath, convertError);
        if (!converted) {
            importStatus = "Import failed: " + convertError;
            return;
        }
        actualPath = convertedPath;
        ext = fs::path(actualPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (std::find(meshExts.begin(), meshExts.end(), ext) != meshExts.end()) {
        BeginMeshImport(actualPath);
    } else if (std::find(texExts.begin(), texExts.end(), ext) != texExts.end()) {
        CookImportedAsset(actualPath, "Texture", ".startex");
    } else if (std::find(soundExts.begin(), soundExts.end(), ext) != soundExts.end()) {
        CookImportedAsset(actualPath, "Sound", ".starsound");
    } else {
        importStatus = "Import failed: unrecognized file type " + ext;
    }
}

void ContentBrowserPanel::ReimportFromFilePrompt(const std::string& starAssetPath, const std::string& displayName) {
    StarAsset asset;
    std::string description = "All Files";
    std::string filter = "*.*";
    if (StarAsset::LoadFromFile(starAssetPath, asset)) {
        if (asset.assetType == "StaticMesh" || asset.assetType == "Animation") {
            description = "Mesh (obj, gltf, glb, iqm)";
            filter = "*.obj;*.gltf;*.glb;*.iqm";
        } else if (asset.assetType == "Texture") {
            description = "Image (png, jpg, bmp)";
            filter = "*.png;*.jpg;*.jpeg;*.bmp";
        } else if (asset.assetType == "Sound") {
            description = "Sound (wav)";
            filter = "*.wav";
        }
    }

    std::string path = NativeFileDialog::OpenFile("Reimport From File", "", description, filter);
    if (path.empty()) return;

    std::string result = AssetImporter::ReimportFromFile(starAssetPath, path);
    importStatus = result.empty() ? "Reimport failed" : "Reimported " + displayName;
    tilesDirty = true;
}

void ContentBrowserPanel::ImportAssetAuto() {
    std::string description = "Mesh, Texture or Sound (obj, gltf, glb, iqm, png, jpg, bmp, wav)";
    std::string filter = "*.obj;*.gltf;*.glb;*.iqm;*.png;*.jpg;*.jpeg;*.bmp;*.wav";
    for (const SourceFormatConverter& converter : SourceFormatConverterRegistry::Get().GetAvailableConverters()) {
        std::string extNoDot = converter.extension.empty() ? "" : converter.extension.substr(1);
        description.insert(description.size() - 1, ", " + extNoDot);
        filter += ";*" + converter.extension;
    }

    std::string path = NativeFileDialog::OpenFile("Import Asset", "", description, filter);
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

void ContentBrowserPanel::BuildMaterialEditorChrome() {
    materialChrome = new Widget();
    auto* panel = new RectangleElement();
    panel->cornerRadius = 0.05f;
    materialChrome->SetRoot(panel);

    const UIPalette& theme = UITheme::Get();
    panel->color = theme.panelBg;

    materialTitle = new TextElement();
    materialTitle->fontSize = 13.0f;
    materialTitle->color = theme.textPrimary;
    materialTitle->anchor = { 0.0f, 0.0f };
    materialTitle->pivot = { 0.0f, 0.0f };
    materialTitle->offset = { 8.0f, 6.0f };
    materialTitle->size = { 0.0f, 16.0f };
    panel->AddChild(materialTitle);

    auto* colorLabel = new TextElement();
    colorLabel->text = "Base Color";
    colorLabel->fontSize = 11.0f;
    colorLabel->color = theme.textSecondary;
    colorLabel->anchor = { 0.0f, 0.0f };
    colorLabel->pivot = { 0.0f, 0.0f };
    colorLabel->offset = { 8.0f, 28.0f };
    colorLabel->size = { 0.0f, 14.0f };
    panel->AddChild(colorLabel);

    const char* channelLabels[4] = {"R", "G", "B", "A"};
    float chW = (320.0f - 16.0f - 3.0f * 6.0f) / 4.0f;
    for (int i = 0; i < 4; i++) {
        float cx = 8.0f + i * (chW + 6.0f);

        auto* chLabel = new TextElement();
        chLabel->text = channelLabels[i];
        chLabel->fontSize = 10.0f;
        chLabel->color = theme.textSecondary;
        chLabel->anchor = { 0.0f, 0.0f };
        chLabel->pivot = { 0.0f, 0.0f };
        chLabel->offset = { cx, 44.0f };
        chLabel->size = { chW, 12.0f };
        panel->AddChild(chLabel);

        auto* chBox = new TextElement();
        chBox->editable = true;
        chBox->numericOnly = true;
        chBox->fontSize = 11.0f;
        chBox->color = theme.textPrimary;
        chBox->backgroundColor = theme.fieldBg;
        chBox->borderColor = theme.fieldBorder;
        chBox->editingBorderColor = theme.accent;
        chBox->anchor = { 0.0f, 0.0f };
        chBox->pivot = { 0.0f, 0.0f };
        chBox->offset = { cx, 56.0f };
        chBox->size = { chW, 18.0f };
        int channel = i;
        chBox->onTextChanged = [this, channel](const std::string& text) {
            int v = atoi(text.c_str());
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            unsigned char* channels[4] = {&materialEditorData.baseColor.r, &materialEditorData.baseColor.g,
                                           &materialEditorData.baseColor.b, &materialEditorData.baseColor.a};
            *channels[channel] = (unsigned char)v;
        };
        panel->AddChild(chBox);
        materialChannelBoxes[i] = chBox;
    }

    materialSwatch = new RectangleElement();
    materialSwatch->anchor = { 1.0f, 0.0f };
    materialSwatch->pivot = { 1.0f, 0.0f };
    materialSwatch->offset = { -8.0f, 44.0f };
    materialSwatch->size = { 24.0f, 24.0f };
    materialSwatch->borderThickness = 1.0f;
    materialSwatch->borderColor = theme.fieldBorder;
    panel->AddChild(materialSwatch);

    auto* swatchBtn = new ButtonElement();
    swatchBtn->defaultColor = { 0, 0, 0, 0 };
    swatchBtn->hoveredColor = { 0, 0, 0, 0 };
    swatchBtn->selectedColor = { 0, 0, 0, 0 };
    swatchBtn->pressedColor = { 0, 0, 0, 0 };
    swatchBtn->anchor = { 1.0f, 0.0f };
    swatchBtn->pivot = { 1.0f, 0.0f };
    swatchBtn->offset = { -8.0f, 44.0f };
    swatchBtn->size = { 24.0f, 24.0f };
    swatchBtn->onActivated = [this]() {
        if (!materialColorPicker.IsOpen()) {
            Vector2 pos = swatchBtn2Pos;
            materialColorPicker.Open(pos, materialEditorData.baseColor);
        }
    };
    panel->AddChild(swatchBtn);

    materialLitLabel = new TextElement();
    materialLitLabel->text = "Lit";
    materialLitLabel->fontSize = 11.0f;
    materialLitLabel->anchor = { 0.0f, 0.0f };
    materialLitLabel->pivot = { 0.0f, 0.0f };
    materialLitLabel->offset = { 8.0f, 82.0f };
    materialLitLabel->size = { 0.0f, 16.0f };
    panel->AddChild(materialLitLabel);

    materialLitBox = new CheckBoxElement();
    materialLitBox->anchor = { 1.0f, 0.0f };
    materialLitBox->pivot = { 1.0f, 0.0f };
    materialLitBox->offset = { -8.0f, 80.0f };
    materialLitBox->size = { 18.0f, 18.0f };
    materialLitBox->onToggled = [this](bool value) { materialEditorData.lit = value; };
    panel->AddChild(materialLitBox);

    auto* texLabel = new TextElement();
    texLabel->text = "Texture";
    texLabel->fontSize = 11.0f;
    texLabel->color = theme.textSecondary;
    texLabel->anchor = { 0.0f, 0.0f };
    texLabel->pivot = { 0.0f, 0.0f };
    texLabel->offset = { 8.0f, 104.0f };
    texLabel->size = { 0.0f, 14.0f };
    panel->AddChild(texLabel);

    auto* texRow = new ButtonElement();
    texRow->defaultColor = theme.fieldBg;
    texRow->hoveredColor = theme.surfaceHover;
    texRow->selectedColor = theme.surfaceHover;
    texRow->pressedColor = theme.surfaceHover;
    texRow->anchor = { 0.0f, 0.0f };
    texRow->pivot = { 0.0f, 0.0f };
    texRow->offset = { 8.0f, 118.0f };
    texRow->size = { 304.0f, 18.0f };
    texRow->onActivated = [this]() {
        if (!materialTexturePicker.IsOpen()) {
            materialTexturePicker.Open(texRowScreenPos, ".startex");
        }
    };
    panel->AddChild(texRow);

    materialTexLabel = new TextElement();
    materialTexLabel->fontSize = 11.0f;
    materialTexLabel->color = theme.textPrimary;
    materialTexLabel->anchor = { 0.0f, 0.5f };
    materialTexLabel->pivot = { 0.0f, 0.5f };
    materialTexLabel->offset = { 4.0f, 0.0f };
    materialTexLabel->size = { 0.0f, 16.0f };
    texRow->AddChild(materialTexLabel);

    auto* saveBtn = new ButtonElement();
    saveBtn->defaultColor = theme.buttonBg;
    saveBtn->hoveredColor = theme.buttonHover;
    saveBtn->selectedColor = theme.buttonHover;
    saveBtn->pressedColor = theme.buttonPressed;
    saveBtn->anchor = { 0.0f, 0.0f };
    saveBtn->pivot = { 0.0f, 0.0f };
    saveBtn->offset = { 8.0f, 194.0f };
    saveBtn->size = { 150.0f, 22.0f };
    saveBtn->onActivated = [this]() {
        std::string materialBefore, materialAfter;
        ContentUndo::ReadFileBytes(materialEditorPath, materialBefore);
        materialEditorData.SaveToFile(materialEditorPath);
        ContentUndo::ReadFileBytes(materialEditorPath, materialAfter);
        std::string undoMaterialPath = materialEditorPath;
        ContentUndo::RecordFileEdit(undoMaterialPath, materialBefore, materialAfter, "Material Edit",
                                    [undoMaterialPath]() { AssetCache::Get().ReloadMaterial(undoMaterialPath); });
        AssetCache::Get().ReloadMaterial(materialEditorPath);
        materialEditorOpen = false;
    };
    auto* saveLabel = new TextElement();
    saveLabel->text = "Save";
    saveLabel->fontSize = 12.0f;
    saveLabel->color = theme.buttonText;
    saveLabel->anchor = { 0.5f, 0.5f };
    saveLabel->pivot = { 0.5f, 0.5f };
    saveBtn->AddChild(saveLabel);
    panel->AddChild(saveBtn);

    auto* cancelBtn = new ButtonElement();
    cancelBtn->defaultColor = theme.buttonBg;
    cancelBtn->hoveredColor = theme.buttonHover;
    cancelBtn->selectedColor = theme.buttonHover;
    cancelBtn->pressedColor = theme.buttonPressed;
    cancelBtn->anchor = { 0.0f, 0.0f };
    cancelBtn->pivot = { 0.0f, 0.0f };
    cancelBtn->offset = { 162.0f, 194.0f };
    cancelBtn->size = { 150.0f, 22.0f };
    cancelBtn->onActivated = [this]() { materialEditorOpen = false; };
    auto* cancelLabel = new TextElement();
    cancelLabel->text = "Cancel";
    cancelLabel->fontSize = 12.0f;
    cancelLabel->color = theme.buttonText;
    cancelLabel->anchor = { 0.5f, 0.5f };
    cancelLabel->pivot = { 0.5f, 0.5f };
    cancelBtn->AddChild(cancelLabel);
    panel->AddChild(cancelBtn);
}

void ContentBrowserPanel::OpenMaterialEditor(const std::string& fullPath) {
    materialEditorPath = fullPath;
    materialEditorData = StarMaterial{};
    StarMaterial::LoadFromFile(fullPath, materialEditorData);
    materialTexturePicker.Close();
    materialColorPicker.Close();
    materialEditorOpen = true;
    delete materialChrome;
    materialChrome = nullptr;
}

void ContentBrowserPanel::RenderMaterialEditor() {
    if (!materialEditorOpen) return;
    if (!materialChrome) BuildMaterialEditorChrome();

    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = materialTexturePicker.ContainsPoint(mp) || materialColorPicker.ContainsPoint(mp);
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    Rectangle box = {screenW * 0.5f - 160, screenH * 0.5f - 111, 320, 222};

    DrawRectangleRec({0, 0, screenW, screenH}, {0, 0, 0, 120});

    materialTitle->text = fs::path(materialEditorPath).stem().string();

    if (materialColorPicker.IsOpen()) {
        Color live = materialColorPicker.GetLiveColor();
        materialEditorData.baseColor = live;
    }

    unsigned char* channels[4] = {&materialEditorData.baseColor.r, &materialEditorData.baseColor.g,
                                   &materialEditorData.baseColor.b, &materialEditorData.baseColor.a};
    for (int i = 0; i < 4; i++) {
        if (!materialChannelBoxes[i]->isEditing) {
            materialChannelBoxes[i]->text = std::to_string((int)*channels[i]);
        }
    }

    materialSwatch->color = materialEditorData.baseColor;
    swatchBtn2Pos = { box.x + box.width - 34, box.y + 28 + 12 + 34 };

    bool shadingAllowsLit = ShadingModel::ShouldUseLitShading();
    const UIPalette& theme = UITheme::Get();
    materialLitLabel->color = shadingAllowsLit ? theme.textSecondary : theme.buttonDisabledText;
    materialLitBox->checked = materialEditorData.lit;
    materialLitBox->disabled = !shadingAllowsLit;

    materialTexLabel->text = materialEditorData.textureAssetPath.empty()
                                  ? "None"
                                  : fs::path(materialEditorData.textureAssetPath).stem().string();
    texRowScreenPos = { box.x + 8, box.y + 118 + 18 };

    if (materialColorPicker.IsOpen()) {
        Color pickerResult;
        bool pickerConfirmed = false;
        if (materialColorPicker.Render(pickerResult, pickerConfirmed)) {
            if (pickerConfirmed) materialEditorData.baseColor = pickerResult;
        }
    }

    std::string pickedTex;
    if (materialTexturePicker.Render(pickedTex)) materialEditorData.textureAssetPath = pickedTex;

    float dt = GetFrameTime();
    materialChrome->Tick(dt, box, 1.0f);
    if (!pickerBlocksInput) {
        WidgetInput::ProcessInput(materialChrome, WidgetInput::kEditorDevice, dt, mp, 1.0f);
    }
    materialChrome->Draw(box, 1.0f);
}

void ContentBrowserPanel::BuildChrome() {
    chrome = new Widget();
    chromeRoot = new VerticalBoxElement();
    chromeRoot->childSpacing = 0.0f;
    chrome->SetRoot(chromeRoot);

    breadcrumbBar = new HorizontalBoxElement();
    breadcrumbBar->childSpacing = 2.0f;
    breadcrumbBar->size = { 0.0f, 22.0f };
    chromeRoot->AddChild(breadcrumbBar);

    gridScroll = new BoxElement();
    gridScroll->axis = BoxAxis::Vertical;
    gridScroll->scrollEnabled = true;
    gridScroll->backgroundColor = { 0, 0, 0, 0 };
    chromeRoot->AddChild(gridScroll);

    tileGrid = new GridElement();
    tileGrid->cellSpacing = 8.0f;
    gridScroll->AddChild(tileGrid);
}

void ContentBrowserPanel::RebuildBreadcrumb(const std::vector<std::pair<std::string, std::string>>& crumbs) {
    const UIPalette& theme = UITheme::Get();
    breadcrumbBar->ClearChildren();

    auto* importBtn = new ButtonElement();
    importBtn->size = { 18.0f, 18.0f };
    importBtn->defaultColor = theme.buttonBg;
    importBtn->hoveredColor = theme.buttonHover;
    importBtn->selectedColor = theme.buttonHover;
    importBtn->pressedColor = theme.buttonPressed;
    importBtn->onActivated = [this]() { ImportAssetAuto(); };
    auto* importLabel = new TextElement();
    importLabel->text = "+";
    importLabel->fontSize = 12.0f;
    importLabel->color = theme.buttonText;
    importLabel->anchor = { 0.5f, 0.5f };
    importLabel->pivot = { 0.5f, 0.5f };
    importBtn->AddChild(importLabel);
    breadcrumbBar->AddChild(importBtn);

    for (size_t i = 0; i < crumbs.size(); i++) {
        if (i > 0) {
            auto* sep = new TextElement();
            sep->text = "/";
            sep->fontSize = 12.0f;
            sep->color = theme.textSecondary;
            sep->size = { 8.0f, 18.0f };
            sep->anchor = { 0.5f, 0.5f };
            sep->pivot = { 0.5f, 0.5f };
            breadcrumbBar->AddChild(sep);
        }

        std::string target = crumbs[i].second;
        float labelW = (float)MeasureText(crumbs[i].first.c_str(), 12) + 10.0f;

        auto* crumbBtn = new ButtonElement();
        crumbBtn->size = { labelW, 18.0f };
        crumbBtn->defaultColor = { 0, 0, 0, 0 };
        crumbBtn->hoveredColor = theme.surfaceHover;
        crumbBtn->selectedColor = theme.surfaceHover;
        crumbBtn->pressedColor = theme.surfaceHover;
        crumbBtn->onActivated = [this, target]() { NavigateToRelativeFolder(target); };
        crumbBtn->elementId = "crumb:" + target;

        auto* crumbLabel = new TextElement();
        crumbLabel->text = crumbs[i].first;
        crumbLabel->fontSize = 12.0f;
        crumbLabel->color = theme.textPrimary;
        crumbLabel->anchor = { 0.5f, 0.5f };
        crumbLabel->pivot = { 0.5f, 0.5f };
        crumbBtn->AddChild(crumbLabel);

        breadcrumbBar->AddChild(crumbBtn);
    }
}

void ContentBrowserPanel::RebuildTiles(const std::vector<ContentEntry>& entries) {
    const UIPalette& theme = UITheme::Get();
    tileGrid->ClearChildren();
    tileButtons.clear();

    for (size_t i = 0; i < entries.size(); i++) {
        const ContentEntry& entry = entries[i];
        bool isSelected = (selectedIndex == (int)i);

        auto* tile = new ButtonElement();
        tile->selectionKey = entry.fullDiskName;
        tile->size = { 0.0f, 0.0f };
        tile->cornerRadius = 0.1f;
        tile->defaultColor = theme.buttonBg;
        tile->hoveredColor = theme.buttonHover;
        tile->selectedColor = theme.buttonHover;
        tile->pressedColor = theme.buttonPressed;
        if (isSelected) {
            tile->borderThickness = 1.5f;
            tile->borderColor = theme.accent;
        }

        std::string diskName = entry.fullDiskName;
        bool isFolder = entry.isFolder;
        std::string extension = entry.extension;
        std::string displayName = entry.displayName;
        int index = (int)i;

        tile->onActivated = [this, index, diskName, isFolder, extension, displayName]() {
            float now = (float)GetTime();
            if (lastEntryClickIndex == index && (now - lastEntryClickTime) < 0.3f) {
                ContentEntry target;
                target.displayName = displayName;
                target.fullDiskName = diskName;
                target.isFolder = isFolder;
                target.extension = extension;
                OpenEntry(target);
            } else {
                selectedIndex = index;
                if (!isFolder) {
                    fs::path fullPath = fs::path(GetCurrentFolderPath()) / diskName;
                    DragDrop::Begin("ContentAsset", fullPath.string(), displayName, GetMousePosition());
                }
            }
            lastEntryClickTime = now;
            lastEntryClickIndex = index;
        };

        tile->onKeyActivated = [this, diskName, isFolder, extension, displayName]() {
            ContentEntry target;
            target.displayName = displayName;
            target.fullDiskName = diskName;
            target.isFolder = isFolder;
            target.extension = extension;
            OpenEntry(target);
        };

        tile->onRightActivated = [this, index, diskName, isFolder, extension, displayName]() {
            selectedIndex = index;
            fs::path fullPath = fs::path(GetCurrentFolderPath()) / diskName;
            contextTargetPath = fullPath.string();
            contextTargetDisplayName = displayName;
            contextTargetExtension = extension;
            contextTargetIsFolder = isFolder;
            OpenItemContextMenu(GetMousePosition());
        };

        auto* icon = new ImageElement();
        icon->directTexture = GetIconTexturePtr(entry);
        icon->anchor = { 0.5f, 0.0f };
        icon->pivot = { 0.5f, 0.0f };
        icon->offset = { 0.0f, 4.0f };
        icon->size = { tileSize - 16.0f, tileSize - 26.0f };
        tile->AddChild(icon);

        if (!icon->directTexture) {
            auto* fallback = new RectangleElement();
            fallback->color = isFolder ? Color{210, 180, 90, 255} : Color{90, 150, 210, 255};
            fallback->anchor = { 0.5f, 0.0f };
            fallback->pivot = { 0.5f, 0.0f };
            fallback->offset = { 0.0f, tileSize * 0.2f };
            fallback->size = { tileSize * 0.4f, tileSize * 0.4f };
            tile->AddChild(fallback);
        }

        auto* label = new TextElement();
        label->text = entry.displayName;
        label->fontSize = 10.0f;
        label->color = theme.textPrimary;
        label->anchor = { 0.5f, 1.0f };
        label->pivot = { 0.5f, 1.0f };
        label->offset = { 0.0f, -2.0f };
        label->size = { tileSize - 4.0f, 16.0f };

        if (isRenamingItem && diskName == renamingOriginalDiskName) {
            label->editable = true;
            label->onTextCommitted = [this](const std::string& newName) {
                snprintf(nameBuffer, sizeof(nameBuffer), "%s", newName.c_str());
                ConfirmRename();
                tilesDirty = true;
            };
            label->onEditCancelled = [this]() {
                CancelRename();
                tilesDirty = true;
            };
        }
        tile->AddChild(label);

        tileGrid->AddChild(tile);
        tileButtons.push_back({ tile, diskName, isFolder });

        if (isRenamingItem && diskName == renamingOriginalDiskName) label->StartEditing();
    }
}

void ContentBrowserPanel::OpenItemContextMenu(Vector2 pos) {
    bool showReimport = !contextTargetIsFolder &&
        (contextTargetExtension == ".starasset" || contextTargetExtension == ".startex" ||
         contextTargetExtension == ".starsound" || contextTargetExtension == ".staranim");
    bool showPaste = !clipboardPath.empty();

    itemMenuActions.clear();
    std::vector<ChoiceOption> options;
    auto add = [&](const char* label, const char* action) {
        options.push_back({ label });
        itemMenuActions.push_back(action);
    };

    add("Open", "Open");
    add("Rename", "Rename");
    add("Duplicate", "Duplicate");
    add("Copy", "Copy");
    if (showPaste) add("Paste", "Paste");
    add("Copy Path", "CopyPath");
    if (showReimport) add("Reimport", "Reimport");
    if (showReimport) add("Reimport from File...", "ReimportFromFile");
    add("View in Explorer", "Reveal");
    add("Delete", "Delete");

    itemContextMenu.Open(pos, 150.0f, options);
}

void ContentBrowserPanel::OpenGridContextMenu(Vector2 pos) {
    gridMenuExtensions.clear();
    std::vector<ChoiceOption> options;
    options.push_back({ "New Folder" });
    gridMenuExtensions.push_back(std::string());

    for (const AssetTypeInfo& info : AssetTypeRegistry::Get().GetFilteredTypes()) {
        if (!info.createAsset) continue;
        options.push_back({ "New " + info.typeName });
        gridMenuExtensions.push_back(info.extension);
    }

    gridContextMenu.Open(pos, 150.0f, options);
}

void ContentBrowserPanel::Render() {
    if (!iconsLoaded && IsWindowReady()) LoadIcons();

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    if (Project::Get().GetRootPath().empty()) {
        DrawText("No Project Loaded", (int)bounds.x + 10, (int)bounds.y + 10, 14, theme.textSecondary);
        return;
    }

    if (!chrome) BuildChrome();

    Rectangle client = { bounds.x, bounds.y, bounds.width, bounds.height };
    Vector2 mousePos = GetMousePosition();
    const bool gridInputBlocked = materialEditorOpen || isCreatingNewItem ||
                                   deleteConfirm.IsOpen() || gridContextMenu.IsOpen() || itemContextMenu.IsOpen();

    if (!gridInputBlocked && CheckCollisionPointRec(mousePos, client)) {
        HandleFileDrop();
    }

    Rectangle pathBar = {client.x, client.y, client.width, 22};
    DrawRectangleRec(pathBar, theme.tabBar);

    std::vector<std::pair<std::string, std::string>> crumbs;
    crumbs.push_back({"Content", ""});
    if (!currentRelativeFolder.empty()) {
        size_t pos = 0;
        std::string cumulative;
        while (pos <= currentRelativeFolder.size()) {
            size_t sep = currentRelativeFolder.find('/', pos);
            std::string part = (sep == std::string::npos)
                ? currentRelativeFolder.substr(pos)
                : currentRelativeFolder.substr(pos, sep - pos);
            cumulative = cumulative.empty() ? part : cumulative + "/" + part;
            crumbs.push_back({part, cumulative});
            if (sep == std::string::npos) break;
            pos = sep + 1;
        }
    }

    if (currentRelativeFolder != lastBuiltFolder) {
        RebuildBreadcrumb(crumbs);
        lastBuiltFolder = currentRelativeFolder;
        tilesDirty = true;
    }

    std::vector<ContentEntry> entries = ScanCurrentFolder();

    std::string signature;
    for (const ContentEntry& e : entries) signature += e.fullDiskName + "|";
    signature += "#" + std::to_string(selectedIndex) + "#" + (isRenamingItem ? renamingOriginalDiskName : "");
    if (signature != lastTileSignature || tilesDirty) {
        RebuildTiles(entries);
        lastTileSignature = signature;
        tilesDirty = false;
    }

    float padding = 8.0f;
    float cellSize = tileSize + padding;
    int cols = (int)((client.width - padding) / cellSize);
    if (cols < 1) cols = 1;
    int rows = ((int)entries.size() + cols - 1) / cols;

    tileGrid->columns = cols;
    tileGrid->columnWidth = tileSize;
    tileGrid->rowHeight = tileSize;
    tileGrid->size = { 0.0f, (float)rows * cellSize + padding };

    Rectangle gridArea = {client.x, client.y + 22, client.width, client.height - 22};
    gridScroll->size = { 0.0f, gridArea.height };
    chromeRoot->backgroundColor = { 0, 0, 0, 0 };
    chromeRoot->size = { client.width, client.height };

    float dt = GetFrameTime();
    Rectangle inner = { client.x + padding, client.y, client.width - padding * 2.0f, client.height };
    chrome->Tick(dt, inner, 1.0f);
    if (!gridInputBlocked) {
        WidgetInput::ProcessInput(chrome, "Editor", dt, mousePos, 1.0f);
    }
    chrome->Draw(inner, 1.0f);

    if (DragDrop::Matches("ContentAsset")) {
        const std::string& draggedPath = DragDrop::Payload();
        std::string destFolder;

        for (const TileRef& tile : tileButtons) {
            if (!tile.isFolder) continue;
            if (!CheckCollisionPointRec(mousePos, tile.button->lastResolvedRect)) continue;
            DrawRectangleRoundedLinesEx(tile.button->lastResolvedRect, 0.1f, 4, 2.0f, {110, 200, 130, 255});
            destFolder = (fs::path(GetCurrentFolderPath()) / tile.diskName).string();
            break;
        }

        if (destFolder.empty()) {
            for (WidgetElement* child : breadcrumbBar->GetChildren()) {
                if (child->elementId.rfind("crumb:", 0) != 0) continue;
                if (!CheckCollisionPointRec(mousePos, child->lastResolvedRect)) continue;
                DrawRectangleLinesEx(child->lastResolvedRect, 2.0f, {110, 200, 130, 255});
                std::string rel = child->elementId.substr(6);
                fs::path root = fs::path(Project::Get().GetRootPath()) / "Content";
                destFolder = rel.empty() ? root.string() : (root / rel).string();
                break;
            }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (!destFolder.empty()) {
                fs::path destPath = fs::path(destFolder) / fs::path(draggedPath).filename();
                std::error_code ec;
                if (fs::exists(destPath, ec)) {
                    importStatus = "Move failed: " + destPath.filename().string() + " already exists there";
                } else {
                    fs::rename(draggedPath, destPath, ec);
                    if (!ec) ContentUndo::RecordMove(draggedPath, destPath, "Move");
                    importStatus = !ec ? "Moved " + destPath.stem().string()
                                        : "Move failed: " + ec.message();
                    tilesDirty = true;
                }
            } else if (onAssetDragged && !CheckCollisionPointRec(mousePos, client)) {
                std::string ext = fs::path(draggedPath).extension().string();
                onAssetDragged(draggedPath, ext, mousePos);
            }
            DragDrop::Consume();
        }
    }

    if (isCreatingNewItem) {
        Rectangle createRect = { client.x + padding, client.y + client.height - 48.0f, tileSize, 20.0f };
        if (!createNameChrome) {
            createNameChrome = new Widget();
            createNameBox = new TextElement();
            createNameBox->editable = true;
            createNameBox->text = nameBuffer;
            createNameChrome->SetRoot(createNameBox);
            createNameBox->onTextCommitted = [this](const std::string& text) {
                snprintf(nameBuffer, sizeof(nameBuffer), "%s", text.c_str());
                ConfirmPendingCreate();
                tilesDirty = true;
            };
            createNameBox->onEditCancelled = [this]() {
                CancelPendingCreate();
                tilesDirty = true;
            };
            createNameBox->StartEditing();
        }

        const UIPalette& theme2 = UITheme::Get();
        createNameBox->fontSize = 11.0f;
        createNameBox->color = theme2.textPrimary;
        createNameBox->backgroundColor = theme2.fieldBg;
        createNameBox->borderColor = theme2.fieldBorder;
        createNameBox->editingBorderColor = theme2.accent;

        float dt2 = GetFrameTime();
        createNameChrome->Tick(dt2, createRect, 1.0f);
        WidgetInput::ProcessInput(createNameChrome, WidgetInput::kEditorDevice, dt2, mousePos, 1.0f);
        createNameChrome->Draw(createRect, 1.0f);
    } else if (createNameChrome) {
        delete createNameChrome;
        createNameChrome = nullptr;
        createNameBox = nullptr;
    }

    if (!gridInputBlocked && !isRenamingItem && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
        CheckCollisionPointRec(mousePos, gridArea)) {
        bool overTile = false;
        for (const TileRef& tile : tileButtons) {
            if (CheckCollisionPointRec(mousePos, tile.button->lastResolvedRect)) { overTile = true; break; }
        }
        if (!overTile) OpenGridContextMenu(mousePos);
    }

    int gridChoice = -1;
    if (gridContextMenu.Render(gridChoice)) {
        if (gridChoice >= 0 && gridChoice < (int)gridMenuExtensions.size()) {
            const std::string& chosenExtension = gridMenuExtensions[gridChoice];
            if (chosenExtension.empty()) {
                BeginCreateFolder();
            } else if (const AssetTypeInfo* chosenInfo = AssetTypeRegistry::Get().FindByExtension(chosenExtension)) {
                BeginCreateAsset(*chosenInfo);
            }
        }
    }

    int itemChoice = -1;
    if (itemContextMenu.Render(itemChoice)) {
        if (itemChoice >= 0 && itemChoice < (int)itemMenuActions.size()) {
            const std::string& action = itemMenuActions[itemChoice];

            ContentEntry target;
            target.displayName = contextTargetDisplayName;
            target.fullDiskName = contextTargetIsFolder ? contextTargetDisplayName
                                                        : fs::path(contextTargetPath).filename().string();
            target.isFolder = contextTargetIsFolder;
            target.extension = contextTargetExtension;

            if (action == "Open") {
                OpenEntry(target);
            } else if (action == "Rename") {
                BeginRename(target);
                tilesDirty = true;
            } else if (action == "Duplicate") {
                DuplicateEntry(contextTargetPath, contextTargetIsFolder);
                tilesDirty = true;
            } else if (action == "Copy") {
                CopyToClipboard(contextTargetPath, contextTargetIsFolder);
            } else if (action == "Paste") {
                PasteFromClipboard();
                tilesDirty = true;
            } else if (action == "CopyPath") {
                fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
                std::error_code ec;
                fs::path rel = fs::relative(contextTargetPath, contentRoot, ec);
                std::string virtualPath = "Content/" + (ec ? contextTargetPath : rel.generic_string());
                SetClipboardText(virtualPath.c_str());
                importStatus = "Copied path to clipboard";
            } else if (action == "Reimport") {
                if (AssetImporter::NeedsReimportFromFile(contextTargetPath)) {
                    ReimportFromFilePrompt(contextTargetPath, contextTargetDisplayName);
                } else {
                    std::string result = AssetImporter::Reimport(contextTargetPath);
                    importStatus = result.empty() ? "Reimport failed" : "Reimported " + contextTargetDisplayName;
                }
            } else if (action == "ReimportFromFile") {
                ReimportFromFilePrompt(contextTargetPath, contextTargetDisplayName);
            } else if (action == "Reveal") {
                RevealInExplorer(contextTargetPath, contextTargetIsFolder);
            } else if (action == "Delete") {
                RequestDelete(contextTargetPath, contextTargetIsFolder, contextTargetDisplayName);
            }
        }
    }

    if (!gridInputBlocked && selectedIndex >= 0 && selectedIndex < (int)entries.size() && WidgetInput::ScopeHasKeyboard()) {
        if (IsKeyPressed(KEY_DELETE)) {
            const ContentEntry& entry = entries[selectedIndex];
            fs::path fullPath = fs::path(GetCurrentFolderPath()) / entry.fullDiskName;
            RequestDelete(fullPath.string(), entry.isFolder, entry.displayName);
        }
        if (IsKeyPressed(KEY_F2)) {
            BeginRename(entries[selectedIndex]);
            tilesDirty = true;
        }
    }

    bool deleteConfirmed = false;
    if (deleteConfirm.Render(deleteConfirmed)) {
        if (deleteConfirmed) {
            fs::path deleteContentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
            std::set<fs::path> vendorDirsToTrash;
            if (pendingDeleteIsFolder) {
                std::error_code walkEc;
                for (fs::recursive_directory_iterator it(pendingDeletePath, fs::directory_options::skip_permission_denied, walkEc), end;
                     !walkEc && it != end; it.increment(walkEc)) {
                    std::error_code fileEc;
                    if (it->is_regular_file(fileEc)) CollectVendorDirForAsset(it->path(), deleteContentRoot, vendorDirsToTrash);
                }
            } else {
                CollectVendorDirForAsset(fs::path(pendingDeletePath), deleteContentRoot, vendorDirsToTrash);
            }
            TrashVendorDirs(vendorDirsToTrash);
            std::string trashError;
            if (OSTrash::MoveToTrash(pendingDeletePath, trashError)) {
                importStatus = pendingDeleteIsFolder ? "Moved folder to trash" : "Moved to trash";
                selectedIndex = -1;
                tilesDirty = true;
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
