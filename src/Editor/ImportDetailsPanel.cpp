#include "ImportDetailsPanel.h"
#include "Core/StarAsset.h"
#include "PropertyFields.h"
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
    defaultMaterialPath = asset.defaultMaterialPath;
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
    asset.defaultMaterialPath = defaultMaterialPath;

    asset.SaveToFile(assetPath);
}

void ImportDetailsPanel::Open(const std::string& path, const std::string& type) {
    assetPath = path;
    assetType = type;
    activeField = 0;
    materialPicker.Close();
    LoadFromAsset();
    open = true;
}

void ImportDetailsPanel::Close() {
    open = false;
    materialPicker.Close();
}

void ImportDetailsPanel::Render() {
    if (!open) return;

    bool isMesh = (assetType == "StaticMesh");
    float boxWidth = 340;
    float boxHeight = isMesh ? 300 : 120;

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    Rectangle box = {screenW * 0.5f - boxWidth * 0.5f, screenH * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight};

    DrawRectangle(0, 0, (int)screenW, (int)screenH, {0, 0, 0, 140});
    DrawRectangleRec(box, {40, 40, 46, 255});
    DrawRectangleLinesEx(box, 1, {80, 80, 90, 255});

    std::string title = "Import Options - " + fs::path(assetPath).stem().string();
    DrawText(title.c_str(), (int)box.x + 10, (int)box.y + 8, 12, {200, 210, 230, 255});

    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = materialPicker.ContainsPoint(mp);
    float y = box.y + 30;

    auto vec3Row = [&](const char* label, char bufs[3][32], int firstFieldId) {
        DrawText(label, (int)box.x + 10, (int)y, 11, {180, 200, 230, 255});
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

        DrawText("Default Material", (int)box.x + 10, (int)y, 11, {180, 200, 230, 255});
        y += 14;
        Rectangle matRow = {box.x + 10, y, boxWidth - 20, 18};
        DrawRectangleRec(matRow, {30, 30, 32, 255});
        DrawRectangleLinesEx(matRow, 1, {65, 65, 75, 255});
        std::string matLabel = defaultMaterialPath.empty() ? "None" : fs::path(defaultMaterialPath).stem().string();
        DrawText(matLabel.c_str(), (int)matRow.x + 4, (int)matRow.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, matRow) && !pickerBlocksInput) {
            materialPicker.Open({matRow.x, matRow.y + matRow.height}, ".starmat");
        }
        y += 24;
        std::string pickedMat;
        if (materialPicker.Render(pickedMat)) defaultMaterialPath = pickedMat;
    } else {
        DrawText("No import options for this asset type yet.", (int)box.x + 10, (int)y, 11, {150, 150, 160, 255});
        y += 24;
    }

    Rectangle importBtn = {box.x + boxWidth - 154, box.y + boxHeight - 28, 70, 22};
    Rectangle skipBtn = {box.x + boxWidth - 80, box.y + boxHeight - 28, 70, 22};
    if (GuiButton(importBtn, "Import") && !pickerBlocksInput) {
        SaveToAsset();
        Close();
    }
    if (GuiButton(skipBtn, "Skip") && !pickerBlocksInput) {
        Close();
    }
}