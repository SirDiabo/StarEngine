#include "DetailsPanel.h"
#include "Core/StarAsset.h"
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

    auto labeledField = [&](const char* label, char* buf, size_t bufSize, int fieldId, float& outVal) {
        DrawText(label, (int)x + 8, (int)y, 11, {140, 140, 150, 255});
        y += 14;
        Rectangle r = {x + 8, y, w - 16, 18};
        if (activeField == fieldId) {
            GuiTextBox(r, buf, (int)bufSize, true);
            if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, r))) {
                outVal = (float)atof(buf);
                activeField = 0;
            }
        } else {
            bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r);
            if (clicked) GuiTextBox(r, buf, (int)bufSize, false);
            DrawRectangleRec(r, {30, 30, 32, 255});
            DrawRectangleLinesEx(r, 1, {65, 65, 75, 255});
            DrawText(buf, (int)r.x + 4, (int)r.y + 3, 11, WHITE);
            if (clicked) {
                activeField = fieldId;
                snprintf(buf, bufSize, "%.2f", outVal);
            }
        }
        y += 22;
    };

    DrawText(inst.className.c_str(), (int)x + 8, (int)y, 13, WHITE);
    y += 20;

    DrawText("Name", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    Rectangle nameRect = {x + 8, y, w - 16, 18};
    if (activeField == 1) {
        GuiTextBox(nameRect, nameEditBuf, sizeof(nameEditBuf), true);
        if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, nameRect))) {
            inst.name = nameEditBuf;
            activeField = 0;
        }
    } else {
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, nameRect);
        if (clicked) GuiTextBox(nameRect, nameEditBuf, sizeof(nameEditBuf), false);
        DrawRectangleRec(nameRect, {30, 30, 32, 255});
        DrawRectangleLinesEx(nameRect, 1, {65, 65, 75, 255});
        DrawText(nameEditBuf, (int)nameRect.x + 4, (int)nameRect.y + 3, 11, WHITE);
        if (clicked) activeField = 1;
    }
    y += 26;

    DrawText("Position", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    labeledField("X", posXBuf, sizeof(posXBuf), 2, inst.posX);
    labeledField("Y", posYBuf, sizeof(posYBuf), 3, inst.posY);
    labeledField("Z", posZBuf, sizeof(posZBuf), 4, inst.posZ);

    y += 4;
    DrawText("Rotation", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    labeledField("X", rotXBuf, sizeof(rotXBuf), 5, inst.rotX);
    labeledField("Y", rotYBuf, sizeof(rotYBuf), 6, inst.rotY);
    labeledField("Z", rotZBuf, sizeof(rotZBuf), 7, inst.rotZ);

    y += 4;
    DrawText("Scale", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    labeledField("X", scaleXBuf, sizeof(scaleXBuf), 8, inst.scaleX);
    labeledField("Y", scaleYBuf, sizeof(scaleYBuf), 9, inst.scaleY);
    labeledField("Z", scaleZBuf, sizeof(scaleZBuf), 10, inst.scaleZ);

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