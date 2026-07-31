#include "ImportDetailsPanel.h"
#include "Core/StarAsset.h"
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
    assetPath = path;
    assetType = type;
    inlinePanel = inlinePanelMode;
    activeField = 0;
    materialPicker.Close();
    materialSlotPickerIndex = -1;
    LoadFromAsset();
    open = true;
}

void ImportDetailsPanel::Close() {
    open = false;
    inlinePanel = false;
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
    float boxWidth = 340;
    float boxHeight = isMesh ? (196.0f + materialSlots.size() * 24.0f + (materialSlots.empty() ? 0.0f : 38.0f)) : 120.0f;

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
    if (UIWidget::Button(importBtn, "Import") && !pickerBlocksInput) {
        SaveToAsset();
        Close();
    }
    if (UIWidget::Button(skipBtn, "Skip") && !pickerBlocksInput) {
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

    std::string title = "Import Options - " + fs::path(assetPath).stem().string();
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

        if (materialSlots.empty()) {
            DrawText("No material slots (plain .obj, no mtllib)", (int)box.x + 10, (int)y, 10, theme.textSecondary);
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
                std::string matLabel = materialSlots[i].materialPath.empty() ? "None" :
                    fs::path(materialSlots[i].materialPath).stem().string();
                DrawText(matLabel.c_str(), (int)(rowRect.x + rowRect.width * 0.5f), (int)rowRect.y + 3, 10, theme.textPrimary);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rowRect) && !pickerBlocksInput) {
                    materialPicker.Open({rowRect.x, rowRect.y + rowRect.height}, ".starmat");
                    materialSlotPickerIndex = (int)i;
                }
                y += 24;
            }
            std::string pickedMat;
            if (materialPicker.Render(pickedMat)) {
                if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)materialSlots.size()) {
                    materialSlots[materialSlotPickerIndex].materialPath = pickedMat;
                    pendingSave = true;
                }
            }
        }
    } else {
        DrawText("No import options for this asset type yet.", (int)box.x + 10, (int)y, 11, theme.textSecondary);
        y += 24;
    }
}