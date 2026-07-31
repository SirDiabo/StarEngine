#include "DetailsPanel.h"
#include "Core/StarAsset.h"
#include "PropertyFields.h"
#include "UITheme.h"
#include "UIWidgets.h"
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
    materialSlotPickerIndex = -1;
    classComponentsValid = false;
}

void DetailsPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    float x = bounds.x;
    float y = bounds.y + 8;
    float w = bounds.width;
    Vector2 mp = GetMousePosition();

    if (!starMap || !selectedActorIndex || *selectedActorIndex < 0 ||
        *selectedActorIndex >= (int)starMap->GetInstances().size()) {
        DrawText("Nothing selected", (int)x + 8, (int)y, 11, theme.textSecondary);
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

    DrawText(inst.className.c_str(), (int)x + 8, (int)y, 13, theme.textPrimary);
    y += 20;

    DrawText("Name", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle nameRect = {x + 8, y, w - 16, 18};
    std::string instName = inst.name;
    if (PropertyField::StringBox(nameRect, nameEditBuf, sizeof(nameEditBuf), 1, activeField, instName)) {
        inst.name = instName;
    }
    y += 26;

    DrawText("Position", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    vec3Row(posXBuf, posYBuf, posZBuf, 2, inst.posX, inst.posY, inst.posZ);

    y += 4;
    DrawText("Rotation", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    vec3Row(rotXBuf, rotYBuf, rotZBuf, 5, inst.rotX, inst.rotY, inst.rotZ);

    y += 4;
    DrawText("Scale", (int)x + 8, (int)y, 11, theme.textSecondary);
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
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    bool anyPickerOpen = meshPicker.IsOpen() || materialPicker.IsOpen();

    DrawText("Mesh Component", (int)x + 8, (int)y, 12, theme.sectionHeader);
    y += 18;

    DrawText("Static Mesh", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle meshRow = {x + 8, y, w - 16, 18};
    DrawRectangleRec(meshRow, theme.fieldBg);
    DrawRectangleLinesEx(meshRow, 1, theme.fieldBorder);
    DrawText(AssetDisplayName(inst.meshAssetPath).c_str(), (int)meshRow.x + 4, (int)meshRow.y + 3, 11, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, meshRow) && !anyPickerOpen) {
        materialPicker.Close();
        meshPicker.Open({meshRow.x, meshRow.y + meshRow.height}, ".starasset");
    }
    y += 22;
    std::string pickedMesh;
    if (meshPicker.Render(pickedMesh)) inst.meshAssetPath = pickedMesh;

    std::vector<MaterialSlot> meshSlots;
    if (!inst.meshAssetPath.empty()) {
        StarAsset meshAsset;
        if (StarAsset::LoadFromFile(inst.meshAssetPath, meshAsset)) meshSlots = meshAsset.materialSlots;
    }
    if (inst.materialAssetPaths.size() != meshSlots.size()) inst.materialAssetPaths.resize(meshSlots.size());

    bool castsShadow = inst.castsShadow;
    if (GuiCheckBox({x + 8, y, 18, 18}, "Casts Shadow", &castsShadow) && !anyPickerOpen) {
        inst.castsShadow = castsShadow;
    }
    y += 24;

    if (meshSlots.empty()) {
        DrawText("No material slots", (int)x + 8, (int)y, 10, {110, 110, 118, 255});
        y += 18;
        return;
    }

    DrawText("Materials", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    for (size_t i = 0; i < meshSlots.size(); i++) {
        Rectangle matRow = {x + 8, y, w - 16, 18};
        DrawRectangleRec(matRow, theme.fieldBg);
        DrawRectangleLinesEx(matRow, 1, theme.fieldBorder);
        std::string slotName = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
        DrawText(slotName.c_str(), (int)matRow.x + 4, (int)matRow.y + 3, 10, theme.textSecondary);
        std::string matLabel = AssetDisplayName(inst.materialAssetPaths[i]);
        DrawText(matLabel.c_str(), (int)(matRow.x + matRow.width * 0.5f), (int)matRow.y + 3, 10, theme.textPrimary);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, matRow) && !anyPickerOpen) {
            meshPicker.Close();
            materialPicker.Open({matRow.x, matRow.y + matRow.height}, ".starmat");
            materialSlotPickerIndex = (int)i;
        }
        y += 22;
    }
    std::string pickedMat;
    if (materialPicker.Render(pickedMat)) {
        if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)inst.materialAssetPaths.size())
            inst.materialAssetPaths[materialSlotPickerIndex] = pickedMat;
    }
}

void DetailsPanel::RenderClassComponentsSection(const ActorInstance& inst, float x, float& y, float w) {
    const UIPalette& theme = UITheme::Get();

    if (!classComponentsValid || cachedClassPath != inst.graphAssetPath) {
        cachedClassGraph = Stargraph{};
        cachedClassGraph.LoadFromFile(inst.graphAssetPath);
        cachedClassPath = inst.graphAssetPath;
        classComponentsValid = true;
    }

    DrawText("Class", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    DrawText(AssetDisplayName(inst.graphAssetPath).c_str(), (int)x + 8, (int)y, 11, theme.textPrimary);
    y += 18;

    Rectangle editBtn = {x + 8, y, w - 16, 20};
    if (UIWidget::Button(editBtn, "Edit Class")) {
        if (onOpenAsset) onOpenAsset(inst.graphAssetPath, fs::path(inst.graphAssetPath).extension().string());
    }
    y += 28;

    const auto& comps = cachedClassGraph.GetComponents();
    if (comps.empty()) {
        DrawText("No components on this class.", (int)x + 8, (int)y, 11, {110, 110, 118, 255});
        y += 16;
        return;
    }

    DrawText("Components", (int)x + 8, (int)y, 12, theme.sectionHeader);
    y += 18;

    for (const ComponentDecl& comp : comps) {
        Rectangle row = {x + 8, y, w - 16, 18};
        DrawRectangleRec(row, theme.fieldBg);
        std::string label = comp.name + "  (" + comp.componentType + ")";
        DrawText(label.c_str(), (int)row.x + 4, (int)row.y + 3, 11, theme.textPrimary);
        y += 20;
    }

    DrawText("Edit components in the class blueprint.", (int)x + 8, (int)y, 10, {110, 110, 118, 255});
    y += 16;
}