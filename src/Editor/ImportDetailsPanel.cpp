#include "ImportDetailsPanel.h"
#include "Core/StarAsset.h"
#include "Core/Project.h"
#include "AssetImporter.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

ImportDetailsPanel& ImportDetailsPanel::Get() {
    static ImportDetailsPanel instance;
    return instance;
}

void ImportDetailsPanel::LoadFromAsset() {
    StarAsset asset;
    StarAsset::LoadFromFile(assetPath, asset);

    posX = asset.importPosX; posY = asset.importPosY; posZ = asset.importPosZ;
    rotX = asset.importRotX; rotY = asset.importRotY; rotZ = asset.importRotZ;
    scaleX = asset.importScaleX; scaleY = asset.importScaleY; scaleZ = asset.importScaleZ;
    materialSlots = asset.materialSlots;
    hasSkeleton = asset.hasSkeleton;
    skeletonBoneCount = (int)asset.skeleton.size();
}

void ImportDetailsPanel::SaveToAsset() {
    StarAsset asset;
    StarAsset::LoadFromFile(assetPath, asset);

    asset.importPosX = posX; asset.importPosY = posY; asset.importPosZ = posZ;
    asset.importRotX = rotX; asset.importRotY = rotY; asset.importRotZ = rotZ;
    asset.importScaleX = scaleX; asset.importScaleY = scaleY; asset.importScaleZ = scaleZ;
    asset.materialSlots = materialSlots;

    asset.SaveToFile(assetPath);
}

void ImportDetailsPanel::Open(const std::string& path, const std::string& type, bool inlinePanelMode) {
    pendingImport = false;
    assetPath = path;
    assetType = type;
    inlinePanel = inlinePanelMode;
    materialPicker.Close();
    materialSlotPickerIndex = -1;
    LoadFromAsset();
    open = true;
}

void ImportDetailsPanel::OpenForImport(const std::string& srcPath, const std::string& type,
                                        const std::string& extension, const std::string& targetFolder) {
    pendingImport = true;
    pendingSrcPath = srcPath;
    pendingExtension = extension;
    pendingTargetFolder = targetFolder;

    assetPath.clear();
    assetType = type;
    inlinePanel = false;
    materialPicker.Close();
    materialSlotPickerIndex = -1;

    materialSlots.clear();
    hasSkeleton = false;
    skeletonBoneCount = 0;
    hasAnimations = false;
    pendingAnimationNames.clear();
    animationOnly = false;
    animationOnlyTargetMesh.clear();
    compatibleMeshPaths.clear();

    posX = posY = posZ = 0.0f;
    rotX = rotY = rotZ = 0.0f;

    float defaultScale = 1.0f;
    if (type == "StaticMesh") {
        std::string ext = fs::path(srcPath).extension().string();
        for (char& c : ext) c = (char)tolower(c);
        defaultScale = (ext == ".gltf" || ext == ".glb") ? 1.0f : 100.0f;

        AssetImporter::MeshImportPreview preview = AssetImporter::InspectMeshSource(srcPath);
        for (const std::string& n : preview.materialNames) materialSlots.push_back({n, ""});
        hasSkeleton = preview.hasSkeleton;
        skeletonBoneCount = (int)preview.skeletonBoneCount;
        hasAnimations = !preview.animationNames.empty();
        pendingAnimationNames = preview.animationNames;
    }
    scaleX = scaleY = scaleZ = defaultScale;

    if (hasAnimations) {
        std::string contentRoot = Project::Get().GetRootPath() + "/Content";
        std::error_code ec;
        if (fs::exists(contentRoot, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(
                     contentRoot, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) break;
                if (entry.path().extension() != ".starasset") continue;
                StarAsset meshAsset;
                if (!StarAsset::LoadFromFile(entry.path().string(), meshAsset)) continue;
                if (meshAsset.assetType != "StaticMesh" || !meshAsset.hasSkeleton) continue;
                if (skeletonBoneCount > 0 && (int)meshAsset.skeleton.size() != skeletonBoneCount) continue;
                compatibleMeshPaths.push_back(entry.path().string());
            }
        }
        if (!compatibleMeshPaths.empty()) {
            animationOnly = true;
            animationOnlyTargetMesh = compatibleMeshPaths[0];
        }
    }

    open = true;
}

void ImportDetailsPanel::Close() {
    open = false;
    inlinePanel = false;
    pendingImport = false;
    materialPicker.Close();
    materialSlotPickerIndex = -1;
    WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
}

void ImportDetailsPanel::ApplyAndClose() {
    SaveToAsset();
    Close();
}

void ImportDetailsPanel::ApplyImportOrSave() {
    if (pendingImport) {
        if (animationOnly) {
            AssetImporter::CookAnimationsOnly(pendingSrcPath, pendingTargetFolder);
        } else {
            std::string created = AssetImporter::Cook(pendingSrcPath, assetType, pendingExtension, pendingTargetFolder);
            if (!created.empty()) {
                assetPath = created;
                StarAsset freshAsset;
                StarAsset::LoadFromFile(assetPath, freshAsset);
                materialSlots = freshAsset.materialSlots;
                SaveToAsset();
            }
        }
    } else {
        SaveToAsset();
    }
    Close();
}

void ImportDetailsPanel::EnsureChrome() {
    if (chrome) return;
    chrome = new Widget();
    panel = new PropertyPanel();
    chrome->SetRoot(panel->GetRootElement());
    panel->onAssetFieldClicked = [this](const std::string& rowId, const std::string& ext, Vector2 pos) {
        if (rowId.rfind("mat:", 0) == 0) {
            materialSlotPickerIndex = atoi(rowId.c_str() + 4);
            materialPicker.Open(pos, ext);
        }
    };
}

void ImportDetailsPanel::ApplyPanelTheme() {
    const UIPalette& theme = UITheme::Get();
    panel->labelColor = theme.textSecondary;
    panel->headerColor = theme.sectionHeader;
    panel->readOnlyColor = { 110, 110, 118, 255 };
    panel->fieldBg = theme.fieldBg;
    panel->fieldBorder = theme.fieldBorder;
    panel->fieldEditingBorder = theme.accent;
    panel->buttonBg = theme.buttonBg;
    panel->buttonHover = theme.buttonHover;
    panel->checkBoxColor = theme.fieldBg;
    panel->checkBoxHoveredColor = theme.surfaceHover;
    panel->checkBoxBorderColor = theme.fieldBorder;
    panel->checkBoxCheckColor = theme.accent;
}

std::vector<PropertyRow> ImportDetailsPanel::BuildRows(bool modal) {
    std::vector<PropertyRow> rows;
    bool isMesh = (assetType == "StaticMesh");

    if (!isMesh) {
        PropertyRow r;
        r.id = "none";
        r.kind = PropertyRowKind::ReadOnlyText;
        r.stringValue = "No import options for this asset type yet.";
        rows.push_back(r);
    } else {
        auto vec3Row = [&rows](const std::string& id, const std::string& label, float x, float y, float z,
                               std::function<void(float, float, float)> setter) {
            PropertyRow r;
            r.id = id;
            r.kind = PropertyRowKind::Vec3;
            r.label = label;
            r.vec3[0] = x; r.vec3[1] = y; r.vec3[2] = z;
            r.setVec3 = std::move(setter);
            rows.push_back(r);
        };
        vec3Row("pos", "Position", posX, posY, posZ, [this, modal](float x, float y, float z) {
            posX = x; posY = y; posZ = z;
            if (!modal) SaveToAsset();
        });
        vec3Row("rot", "Rotation", rotX, rotY, rotZ, [this, modal](float x, float y, float z) {
            rotX = x; rotY = y; rotZ = z;
            if (!modal) SaveToAsset();
        });
        vec3Row("scale", "Scale", scaleX, scaleY, scaleZ, [this, modal](float x, float y, float z) {
            scaleX = x; scaleY = y; scaleZ = z;
            if (!modal) SaveToAsset();
        });

        if (hasSkeleton) {
            PropertyRow r;
            r.id = "skel";
            r.kind = PropertyRowKind::ReadOnlyText;
            r.label = "Skeleton";
            r.stringValue = std::to_string(skeletonBoneCount) + " bones";
            rows.push_back(r);
        }

        if (materialSlots.empty()) {
            PropertyRow r;
            r.id = "matnone";
            r.kind = PropertyRowKind::ReadOnlyText;
            r.label = "Materials";
            r.stringValue = "No material slots";
            rows.push_back(r);
        } else {
            PropertyRow h;
            h.id = "math";
            h.kind = PropertyRowKind::Header;
            h.label = "Materials";
            rows.push_back(h);
            for (size_t i = 0; i < materialSlots.size(); i++) {
                std::string slotName = materialSlots[i].usemtlName.empty()
                    ? ("Slot " + std::to_string(i)) : materialSlots[i].usemtlName;
                if (pendingImport) {
                    PropertyRow r;
                    r.id = "mat:" + std::to_string(i);
                    r.kind = PropertyRowKind::ReadOnlyText;
                    r.label = slotName;
                    r.stringValue = "(will create)";
                    rows.push_back(r);
                } else {
                    std::string display = materialSlots[i].materialPath.empty()
                        ? "None" : fs::path(materialSlots[i].materialPath).stem().string();
                    PropertyRow r;
                    r.id = "mat:" + std::to_string(i);
                    r.kind = PropertyRowKind::Asset;
                    r.label = slotName;
                    r.assetExtension = ".starmat";
                    r.stringValue = display;
                    rows.push_back(r);
                }
            }
        }

        if (pendingImport && hasAnimations) {
            PropertyRow h;
            h.id = "animh";
            h.kind = PropertyRowKind::Header;
            h.label = "Animations";
            rows.push_back(h);
            for (size_t i = 0; i < pendingAnimationNames.size(); i++) {
                PropertyRow r;
                r.id = "anim:" + std::to_string(i);
                r.kind = PropertyRowKind::ReadOnlyText;
                r.stringValue = pendingAnimationNames[i];
                rows.push_back(r);
            }
            PropertyRow b;
            b.id = "animonly";
            b.kind = PropertyRowKind::Bool;
            b.label = "Only Import Animation";
            b.boolValue = animationOnly;
            b.setBool = [this](bool v) { animationOnly = v; };
            rows.push_back(b);
            if (animationOnly) {
                PropertyRow r;
                r.id = "animtarget";
                r.kind = PropertyRowKind::ReadOnlyText;
                r.label = "Target Mesh";
                r.stringValue = animationOnlyTargetMesh.empty()
                    ? "No compatible mesh found" : fs::path(animationOnlyTargetMesh).stem().string();
                rows.push_back(r);
            }
        }
    }

    if (modal) {
        PropertyRow buttons;
        buttons.id = "buttons";
        buttons.kind = PropertyRowKind::Cells;
        PropertyCell skip;
        skip.kind = PropertyCellKind::Button;
        skip.text = pendingImport ? "Cancel" : "Skip";
        skip.onPressed = [this](Rectangle) { Close(); };
        PropertyCell apply;
        apply.kind = PropertyCellKind::Button;
        apply.text = pendingImport ? "Import" : "Apply";
        apply.onPressed = [this](Rectangle) { ApplyImportOrSave(); };
        buttons.cells = { skip, apply };
        rows.push_back(buttons);
    }

    return rows;
}

void ImportDetailsPanel::Render() {
    if (!open || inlinePanel) return;
    EnsureChrome();

    std::string titleStem = pendingImport ? fs::path(pendingSrcPath).stem().string() : fs::path(assetPath).stem().string();
    std::string title = (pendingImport ? "Import - " : "Import Options - ") + titleStem;

    std::vector<PropertyRow> rows = BuildRows(true);
    ApplyPanelTheme();
    panel->SetProperties(rows);

    float titleH = 30.0f;
    float boxWidth = 340.0f;
    float boxHeight = titleH + (float)rows.size() * panel->rowHeight + 14.0f;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    Rectangle box = { screenW * 0.5f - boxWidth * 0.5f, screenH * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight };
    Rectangle body = { box.x + 6.0f, box.y + titleH, boxWidth - 12.0f, boxHeight - titleH - 6.0f };

    DrawRectangle(0, 0, (int)screenW, (int)screenH, { 0, 0, 0, 140 });
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(box, theme.panelBg);
    DrawRectangleLinesEx(box, 1, theme.panelBorder);
    DrawText(title.c_str(), (int)box.x + 10, (int)box.y + 8, 12, theme.sectionHeader);

    WidgetInput::PushModal(this, WidgetInput::kEditorDevice);
    WidgetInput::SetInputScope(this);

    bool pickerOpen = materialPicker.IsOpen();
    float dt = GetFrameTime();
    chrome->Tick(dt, body, 1.0f);
    if (!pickerOpen) {
        WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    }
    chrome->Draw(body, 1.0f);
    WidgetInput::SetInputScope(nullptr);

    std::string picked;
    if (materialPicker.Render(picked)) {
        if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)materialSlots.size()) {
            materialSlots[materialSlotPickerIndex].materialPath = picked;
        }
    }

}

void ImportDetailsPanel::RenderInline(Rectangle area) {
    if (!open || !inlinePanel) return;
    EnsureChrome();

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(area, theme.panelBg);
    DrawRectangleLinesEx(area, 1, theme.panelBorder);
    DrawText("Import Details", (int)area.x + 10, (int)area.y + 6, 12, theme.sectionHeader);
    Rectangle inner = { area.x, area.y + 24, area.width, area.height - 24 };

    std::vector<PropertyRow> rows = BuildRows(false);
    ApplyPanelTheme();
    panel->SetProperties(rows);

    bool pickerOpen = materialPicker.IsOpen();
    float dt = GetFrameTime();
    chrome->Tick(dt, inner, 1.0f);
    if (!pickerOpen) {
        WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    }
    chrome->Draw(inner, 1.0f);

    std::string picked;
    if (materialPicker.Render(picked)) {
        if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)materialSlots.size()) {
            materialSlots[materialSlotPickerIndex].materialPath = picked;
            SaveToAsset();
        }
    }
}
