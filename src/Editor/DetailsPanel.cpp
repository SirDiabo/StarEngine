#include "DetailsPanel.h"
#include "Core/StarAsset.h"
#include "PropertyFields.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

void DetailsPanel::Initialize() {
    name = "Details";
}

static std::string AssetDisplayName(const std::string& assetPath) {
    if (assetPath.empty()) return "None";
    return fs::path(assetPath).stem().string();
}

void DetailsPanel::RefreshFromSelection(const ActorInstance& inst) {
    snprintf(nameEditBuf, sizeof(nameEditBuf), "%s", inst.name.c_str());
    snprintf(posXBuf, sizeof(posXBuf), "%.2f", inst.posX);
    snprintf(posYBuf, sizeof(posYBuf), "%.2f", inst.posY);
    snprintf(posZBuf, sizeof(posZBuf), "%.2f", inst.posZ);
    snprintf(rotXBuf, sizeof(rotXBuf), "%.2f", inst.rotX);
    snprintf(rotYBuf, sizeof(rotYBuf), "%.2f", inst.rotY);
    snprintf(rotZBuf, sizeof(rotZBuf), "%.2f", inst.rotZ);
    snprintf(scaleXBuf, sizeof(scaleXBuf), "%.2f", inst.scaleX);
    snprintf(scaleYBuf, sizeof(scaleYBuf), "%.2f", inst.scaleY);
    snprintf(scaleZBuf, sizeof(scaleZBuf), "%.2f", inst.scaleZ);
    activeField = 0;
    meshPicker.Close();
    materialPicker.Close();
    classComponentsValid = false;
}

void DetailsPanel::Render() {
    DrawRectangleRec(bounds, {38, 38, 42, 255});
    DrawRectangleLinesEx(bounds, 1, {65, 65, 70, 255});

    float x = bounds.x;
    float y = bounds.y + 8;
    float w = bounds.width;
    Vector2 mp = GetMousePosition();

    if (!starMap || !selectedActorIndex || *selectedActorIndex < 0 ||
        *selectedActorIndex >= (int)starMap->GetInstances().size()) {
        DrawText("Nothing selected", (int)x + 8, (int)y, 11, {100, 100, 110, 255});
        return;
    }

    ActorInstance& inst = const_cast<ActorInstance&>(starMap->GetInstances()[*selectedActorIndex]);

    if (lastActorIndex != *selectedActorIndex) {
        RefreshFromSelection(inst);
        lastActorIndex = *selectedActorIndex;
    }

    auto vec3Row = [&](char bufX[32], char bufY[32], char bufZ[32], int fieldBase,
                        float& vx, float& vy, float& vz) {
        Rectangle row = {x + 8, y, w - 16, 18};
        PropertyField::Vec3Row(row, bufX, bufY, bufZ, fieldBase, activeField, vx, vy, vz);
        y += 22;
    };

    DrawText(inst.className.c_str(), (int)x + 8, (int)y, 13, WHITE);
    y += 20;

    DrawText("Name", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    Rectangle nameRect = {x + 8, y, w - 16, 18};
    std::string instName = inst.name;
    if (PropertyField::StringBox(nameRect, nameEditBuf, sizeof(nameEditBuf), 1, activeField, instName)) {
        inst.name = instName;
    }
    y += 26;

    DrawText("Position", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    vec3Row(posXBuf, posYBuf, posZBuf, 2, inst.posX, inst.posY, inst.posZ);

    y += 4;
    DrawText("Rotation", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    vec3Row(rotXBuf, rotYBuf, rotZBuf, 5, inst.rotX, inst.rotY, inst.rotZ);

    y += 4;
    DrawText("Scale", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    vec3Row(scaleXBuf, scaleYBuf, scaleZBuf, 8, inst.scaleX, inst.scaleY, inst.scaleZ);

    y += 6;

    if (!inst.graphAssetPath.empty()) {
        RenderClassComponentsSection(inst, x, y, w);
    } else if (!inst.meshAssetPath.empty()) {
        RenderStaticMeshSection(inst, x, y, w);
    }
}

void DetailsPanel::RenderStaticMeshSection(ActorInstance& inst, float x, float& y, float w) {
    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = meshPicker.ContainsPoint(mp) || materialPicker.ContainsPoint(mp);

    DrawText("Mesh Component", (int)x + 8, (int)y, 12, {180, 200, 230, 255});
    y += 18;

    DrawText("Static Mesh", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    Rectangle meshRow = {x + 8, y, w - 16, 18};
    DrawRectangleRec(meshRow, {30, 30, 32, 255});
    DrawRectangleLinesEx(meshRow, 1, {65, 65, 75, 255});
    DrawText(AssetDisplayName(inst.meshAssetPath).c_str(), (int)meshRow.x + 4, (int)meshRow.y + 3, 11, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, meshRow) && !pickerBlocksInput) {
        materialPicker.Close();
        meshPicker.Open({meshRow.x, meshRow.y + meshRow.height}, ".starasset");
    }
    y += 22;
    std::string pickedMesh;
    if (meshPicker.Render(pickedMesh)) inst.meshAssetPath = pickedMesh;

    DrawText("Material", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    Rectangle matRow = {x + 8, y, w - 16, 18};
    DrawRectangleRec(matRow, {30, 30, 32, 255});
    DrawRectangleLinesEx(matRow, 1, {65, 65, 75, 255});
    DrawText(AssetDisplayName(inst.materialAssetPath).c_str(), (int)matRow.x + 4, (int)matRow.y + 3, 11, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, matRow) && !pickerBlocksInput) {
        meshPicker.Close();
        materialPicker.Open({matRow.x, matRow.y + matRow.height}, ".starmat");
    }
    y += 22;
    std::string pickedMat;
    if (materialPicker.Render(pickedMat)) inst.materialAssetPath = pickedMat;
}

void DetailsPanel::RenderClassComponentsSection(const ActorInstance& inst, float x, float& y, float w) {
    if (!classComponentsValid || cachedClassPath != inst.graphAssetPath) {
        cachedClassGraph = Stargraph{};
        cachedClassGraph.LoadFromFile(inst.graphAssetPath);
        cachedClassPath = inst.graphAssetPath;
        classComponentsValid = true;
    }

    DrawText("Class", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    DrawText(AssetDisplayName(inst.graphAssetPath).c_str(), (int)x + 8, (int)y, 11, WHITE);
    y += 18;

    Rectangle editBtn = {x + 8, y, w - 16, 20};
    if (GuiButton(editBtn, "Edit Class")) {
        if (onOpenAsset) onOpenAsset(inst.graphAssetPath, fs::path(inst.graphAssetPath).extension().string());
    }
    y += 28;

    const auto& comps = cachedClassGraph.GetComponents();
    if (comps.empty()) {
        DrawText("No components on this class.", (int)x + 8, (int)y, 11, {110, 110, 118, 255});
        y += 16;
        return;
    }

    DrawText("Components", (int)x + 8, (int)y, 12, {180, 200, 230, 255});
    y += 18;

    for (const ComponentDecl& comp : comps) {
        Rectangle row = {x + 8, y, w - 16, 18};
        DrawRectangleRec(row, {30, 30, 32, 255});
        std::string label = comp.name + "  (" + comp.componentType + ")";
        DrawText(label.c_str(), (int)row.x + 4, (int)row.y + 3, 11, {210, 210, 215, 255});
        y += 20;
    }

    DrawText("Edit components in the class blueprint.", (int)x + 8, (int)y, 10, {110, 110, 118, 255});
    y += 16;
}