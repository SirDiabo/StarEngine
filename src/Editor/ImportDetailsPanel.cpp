#include "ImportDetailsPanel.h"
#include "Core/StarAsset.h"
#include "Core/Project.h"
#include "AssetImporter.h"
#include "PropertyFields.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
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

    snprintf(posBuf[0], sizeof(posBuf[0]), "%.2f", asset.importPosX);
    snprintf(posBuf[1], sizeof(posBuf[1]), "%.2f", asset.importPosY);
    snprintf(posBuf[2], sizeof(posBuf[2]), "%.2f", asset.importPosZ);
    snprintf(rotBuf[0], sizeof(rotBuf[0]), "%.2f", asset.importRotX);
    snprintf(rotBuf[1], sizeof(rotBuf[1]), "%.2f", asset.importRotY);
    snprintf(rotBuf[2], sizeof(rotBuf[2]), "%.2f", asset.importRotZ);
    snprintf(scaleBuf[0], sizeof(scaleBuf[0]), "%.2f", asset.importScaleX);
    snprintf(scaleBuf[1], sizeof(scaleBuf[1]), "%.2f", asset.importScaleY);
    snprintf(scaleBuf[2], sizeof(scaleBuf[2]), "%.2f", asset.importScaleZ);
    materialSlots = asset.materialSlots;
    hasSkeleton = asset.hasSkeleton;
    skeletonBoneCount = (int)asset.skeleton.size();
}

void ImportDetailsPanel::SaveToAsset() {
    StarAsset asset;
    StarAsset::LoadFromFile(assetPath, asset);

    asset.importPosX = (float)atof(posBuf[0]);
    asset.importPosY = (float)atof(posBuf[1]);
    asset.importPosZ = (float)atof(posBuf[2]);
    asset.importRotX = (float)atof(rotBuf[0]);
    asset.importRotY = (float)atof(rotBuf[1]);
    asset.importRotZ = (float)atof(rotBuf[2]);
    asset.importScaleX = (float)atof(scaleBuf[0]);
    asset.importScaleY = (float)atof(scaleBuf[1]);
    asset.importScaleZ = (float)atof(scaleBuf[2]);
    asset.materialSlots = materialSlots;

    asset.SaveToFile(assetPath);
}

void ImportDetailsPanel::Open(const std::string& path, const std::string& type, bool inlinePanelMode) {
    pendingImport = false;
    assetPath = path;
    assetType = type;
    inlinePanel = inlinePanelMode;
    activeField = 0;
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
    activeField = 0;
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

    snprintf(posBuf[0], sizeof(posBuf[0]), "%.2f", 0.0f);
    snprintf(posBuf[1], sizeof(posBuf[1]), "%.2f", 0.0f);
    snprintf(posBuf[2], sizeof(posBuf[2]), "%.2f", 0.0f);
    snprintf(rotBuf[0], sizeof(rotBuf[0]), "%.2f", 0.0f);
    snprintf(rotBuf[1], sizeof(rotBuf[1]), "%.2f", 0.0f);
    snprintf(rotBuf[2], sizeof(rotBuf[2]), "%.2f", 0.0f);

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
    snprintf(scaleBuf[0], sizeof(scaleBuf[0]), "%.2f", defaultScale);
    snprintf(scaleBuf[1], sizeof(scaleBuf[1]), "%.2f", defaultScale);
    snprintf(scaleBuf[2], sizeof(scaleBuf[2]), "%.2f", defaultScale);

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
}

void ImportDetailsPanel::ApplyAndClose() {
    SaveToAsset();
    Close();
}

void ImportDetailsPanel::Render() {
    if (!open || inlinePanel) return;

    bool isMesh = (assetType == "StaticMesh");
    float animSectionH = (pendingImport && hasAnimations)
        ? (44.0f + pendingAnimationNames.size() * 14.0f + (animationOnly ? 18.0f : 0.0f))
        : 0.0f;
    float boxWidth = 340;
    float boxHeight = isMesh ? (196.0f + (hasSkeleton ? 20.0f : 0.0f) + materialSlots.size() * 24.0f +
                                 (materialSlots.empty() ? 0.0f : 38.0f) + animSectionH)
                              : 120.0f;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    Rectangle box = {screenW * 0.5f - boxWidth * 0.5f, screenH * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight};

    DrawRectangle(0, 0, (int)screenW, (int)screenH, {0, 0, 0, 140});
    UIWidget::Panel(box);

    RenderBody(box, boxWidth, true);

    Rectangle importBtn = {box.x + boxWidth - 154, box.y + boxHeight - 28, 70, 22};
    Rectangle skipBtn = {box.x + boxWidth - 80, box.y + boxHeight - 28, 70, 22};
    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = materialPicker.IsOpen();
    if (UIWidget::Button(importBtn, pendingImport ? "Import" : "Apply") && !pickerBlocksInput) {
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
    if (UIWidget::Button(skipBtn, pendingImport ? "Cancel" : "Skip") && !pickerBlocksInput) {
        Close();
    }
}

void ImportDetailsPanel::RenderInline(Rectangle area) {
    if (!open || !inlinePanel) return;

    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(area);

    DrawText("Import Details", (int)area.x + 10, (int)area.y + 6, 12, theme.sectionHeader);
    Rectangle inner = {area.x, area.y + 24, area.width, area.height - 24};

    int prevActiveField = activeField;
    RenderBody(inner, inner.width, false);

    if ((prevActiveField != 0 && activeField == 0) || pendingSave) {
        SaveToAsset();
        pendingSave = false;
    }
}

void ImportDetailsPanel::RenderBody(Rectangle box, float boxWidth, bool modal) {
    const UIPalette& theme = UITheme::Get();

    std::string titleStem = pendingImport ? fs::path(pendingSrcPath).stem().string() : fs::path(assetPath).stem().string();
    std::string title = (pendingImport ? "Import - " : "Import Options - ") + titleStem;
    if (modal) DrawText(title.c_str(), (int)box.x + 10, (int)box.y + 8, 12, theme.sectionHeader);

    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = materialPicker.IsOpen();
    float y = box.y + (modal ? 30 : 8);

    bool isMesh = (assetType == "StaticMesh");

    auto vec3Row = [&](const char* label, char bufs[3][32], int firstFieldId) {
        DrawText(label, (int)box.x + 10, (int)y, 11, theme.sectionHeader);
        y += 16;
        Rectangle row = {box.x + 10, y, boxWidth - 20, 18};
        float vx = (float)atof(bufs[0]), vy = (float)atof(bufs[1]), vz = (float)atof(bufs[2]);
        PropertyField::Vec3Row(row, bufs[0], bufs[1], bufs[2], firstFieldId, activeField, vx, vy, vz,
                                !pickerBlocksInput);
        y += 24;
    };

    if (isMesh) {
        vec3Row("Position", posBuf, 1);
        vec3Row("Rotation", rotBuf, 4);
        vec3Row("Scale", scaleBuf, 7);

        if (hasSkeleton) {
            std::string skelLabel = "Skeleton: " + std::to_string(skeletonBoneCount) + " bones";
            DrawText(skelLabel.c_str(), (int)box.x + 10, (int)y, 11, theme.textSecondary);
            y += 20;
        }

        if (materialSlots.empty()) {
            DrawText("No material slots", (int)box.x + 10, (int)y, 10, theme.textSecondary);
            y += 20;
        } else {
            DrawText("Materials", (int)box.x + 10, (int)y, 11, theme.sectionHeader);
            y += 14;
            for (size_t i = 0; i < materialSlots.size(); i++) {
                Rectangle rowRect = {box.x + 10, y, boxWidth - 20, 18};
                DrawRectangleRec(rowRect, theme.fieldBg);
                DrawRectangleLinesEx(rowRect, 1, theme.fieldBorder);
                std::string slotName = materialSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : materialSlots[i].usemtlName;
                DrawText(slotName.c_str(), (int)rowRect.x + 4, (int)rowRect.y + 3, 10, theme.textSecondary);
                std::string matLabel = materialSlots[i].materialPath.empty() ?
                    (pendingImport ? "(will create)" : "None") :
                    fs::path(materialSlots[i].materialPath).stem().string();
                DrawText(matLabel.c_str(), (int)(rowRect.x + rowRect.width * 0.5f), (int)rowRect.y + 3, 10, theme.textPrimary);
                if (!pendingImport && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                    CheckCollisionPointRec(mp, rowRect) && !pickerBlocksInput) {
                    materialPicker.Open({rowRect.x, rowRect.y + rowRect.height}, ".starmat");
                    materialSlotPickerIndex = (int)i;
                }
                y += 24;
            }
            std::string pickedMat;
            if (!pendingImport && materialPicker.Render(pickedMat)) {
                if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)materialSlots.size()) {
                    materialSlots[materialSlotPickerIndex].materialPath = pickedMat;
                    pendingSave = true;
                }
            }
        }

        if (pendingImport && hasAnimations) {
            DrawText("Animations", (int)box.x + 10, (int)y, 11, theme.sectionHeader);
            y += 14;
            for (const std::string& animName : pendingAnimationNames) {
                DrawText(animName.c_str(), (int)box.x + 14, (int)y, 10, theme.textSecondary);
                y += 14;
            }
            Rectangle animOnlyRow = {box.x + 10, y, 18, 18};
            GuiCheckBox(animOnlyRow, "Only Import Animation", &animationOnly);
            y += 22;
            if (animationOnly) {
                std::string targetLabel = animationOnlyTargetMesh.empty()
                    ? "No compatible mesh found in project"
                    : ("Target mesh: " + fs::path(animationOnlyTargetMesh).stem().string());
                DrawText(targetLabel.c_str(), (int)box.x + 10, (int)y, 10, theme.textSecondary);
                y += 18;
            }
        }
    } else {
        DrawText("No import options for this asset type yet.", (int)box.x + 10, (int)y, 11, theme.textSecondary);
        y += 24;
    }
}