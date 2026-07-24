#include "StargraphEditorPanel.h"
#include "PropertyFields.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/StarAsset.h"
#include "Core/SaveFileRegistry.h"
#include "raygui.h"
#include <algorithm>
#include <cstring>
#include <filesystem>

void StargraphEditorPanel::RenderComponentOutliner(Rectangle area) {
    DrawRectangleRec(area, {28, 28, 31, 255});
    DrawRectangleLinesEx(area, 1, {55, 55, 60, 255});

    Vector2 mousePos = GetMousePosition();
    float y = area.y + 6;

    DrawText("Components", (int)area.x + 8, (int)y, 12, {160, 160, 170, 255});

    // Add Component button.
    Rectangle addBtn = {area.x + area.width - 50, y - 2, 44, 16};
    if (GuiButton(addBtn, "+ Add")) {
        if (addComponentMenu.IsOpen()) addComponentMenu.Close();
        else addComponentMenu.Open({addBtn.x, addBtn.y + addBtn.height});
    }

    y += 18;
    DrawLine((int)area.x + 4, (int)y, (int)(area.x + area.width - 4), (int)y, {55, 55, 60, 255});
    y += 6;

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    float listBottom = area.y + area.height - 4;

    for (size_t i = 0; i < comps.size() && y + 18 <= listBottom; i++) {
        Rectangle row = {area.x + 4, y, area.width - 8, 18};
        bool isSelected = (selectedComponentIndex == (int)i);

        DrawRectangleRec(row, isSelected ? Color{55, 60, 80, 255} : Color{38, 38, 42, 255});
        DrawRectangleRec({area.x + 8, y + 4, 10, 10}, {90, 150, 210, 255});

        std::string label = comps[i].name + " (" + comps[i].componentType + ")";
        DrawText(label.c_str(), (int)area.x + 24, (int)y + 2, 11, WHITE);

        if (CheckCollisionPointRec(mousePos, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selectedComponentIndex = (int)i;
            selectedNodeId = -1;
            selectedNodeIds.clear();
            selectedVariableIndex = -1;
            componentDragIndex = (int)i;
            componentDragActive = false;
            componentDragPos = mousePos;
        }

        if (componentDragIndex >= 0 && componentDragIndex < (int)comps.size() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - componentDragPos.x;
            float dy = mousePos.y - componentDragPos.y;
            if (!componentDragActive && (dx*dx + dy*dy > 25)) {
                componentDragActive = true;
            }
        }

        if (isSelected && IsKeyPressed(KEY_DELETE)) {
            graph.RemoveComponent(i);
            selectedComponentIndex = -1;
            isDirty = true;
            break;
        }

        y += 20;
    }

    if (comps.empty()) {
        DrawText("No components", (int)area.x + 8, (int)y, 11, {100, 100, 108, 255});
    }

    if (addComponentMenu.IsOpen()) {
        static const char* componentTypes[] = { "SceneComponent", "MeshComponent", "CollisionComponent", "AudioComponent", "CameraComponent", "LightComponent", "InputComponent" };
        std::vector<SearchableListPopup::Entry> entries;
        for (const char* typeName : componentTypes) entries.push_back({"Components", typeName, typeName});

        std::string picked = addComponentMenu.Render(entries, area);
        if (!picked.empty()) {
            ComponentDecl comp;
            comp.componentType = picked;
            comp.name          = picked;
            graph.AddComponent(comp);
            selectedComponentIndex = (int)graph.GetComponents().size() - 1;
            selectedNodeId = -1;
            selectedNodeIds.clear();
            selectedVariableIndex = -1;
            isDirty = true;
        }
    }
}

static std::vector<std::string> GetValidParentOptions(const Stargraph& graph, size_t componentIndex) {
    const std::vector<ComponentDecl>& comps = graph.GetComponents();
    const ComponentDecl& self = comps[componentIndex];

    std::vector<std::string> excluded = {self.name};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const ComponentDecl& c : comps) {
            bool parentExcluded = std::find(excluded.begin(), excluded.end(), c.parentName) != excluded.end();
            bool alreadyExcluded = std::find(excluded.begin(), excluded.end(), c.name) != excluded.end();
            if (parentExcluded && !alreadyExcluded) {
                excluded.push_back(c.name);
                changed = true;
            }
        }
    }

    std::vector<std::string> options = {""};
    for (const ComponentDecl& c : comps) {
        if (std::find(excluded.begin(), excluded.end(), c.name) == excluded.end())
            options.push_back(c.name);
    }
    return options;
}

void StargraphEditorPanel::RenderComponentDetails(Rectangle panel, float& y) {
    ComponentDecl& comp = graph.GetComponents()[selectedComponentIndex];
    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = componentMeshPicker.ContainsPoint(mp) || componentMaterialPicker.ContainsPoint(mp)
                            || audioPathPicker.ContainsPoint(mp) || savePicker.ContainsPoint(mp)
                            || inputMappingPicker.ContainsPoint(mp) || inputKeyPicker.ContainsPoint(mp)
                            || inputDevicePicker.ContainsPoint(mp);
    const float kSaveColWidth = 24.0f;

    if (lastComponentDetailIndex != selectedComponentIndex) {
        snprintf(compNameBuf, sizeof(compNameBuf), "%s", comp.name.c_str());
        snprintf(compPosBuf[0], 32, "%.2f", comp.posX);
        snprintf(compPosBuf[1], 32, "%.2f", comp.posY);
        snprintf(compPosBuf[2], 32, "%.2f", comp.posZ);
        snprintf(compRotBuf[0], 32, "%.2f", comp.rotX);
        snprintf(compRotBuf[1], 32, "%.2f", comp.rotY);
        snprintf(compRotBuf[2], 32, "%.2f", comp.rotZ);
        snprintf(compScaleBuf[0], 32, "%.2f", comp.scaleX);
        snprintf(compScaleBuf[1], 32, "%.2f", comp.scaleY);
        snprintf(compScaleBuf[2], 32, "%.2f", comp.scaleZ);
        snprintf(compRadiusBuf, sizeof(compRadiusBuf), "%.2f", comp.collisionRadius);
        snprintf(compHeightBuf, sizeof(compHeightBuf), "%.2f", comp.collisionHeight);
        snprintf(compSoundBuf, sizeof(compSoundBuf), "%s", comp.soundAssetPath.c_str());
        snprintf(compVolumeBuf, sizeof(compVolumeBuf), "%.2f", comp.volume);
        snprintf(compFovBuf, sizeof(compFovBuf), "%.2f", comp.cameraFov);
        snprintf(compLightColorBuf[0], 32, "%.2f", comp.lightColorR);
        snprintf(compLightColorBuf[1], 32, "%.2f", comp.lightColorG);
        snprintf(compLightColorBuf[2], 32, "%.2f", comp.lightColorB);
        snprintf(compLightIntensityBuf, sizeof(compLightIntensityBuf), "%.2f", comp.lightIntensity);
        snprintf(compLightRadiusBuf, sizeof(compLightRadiusBuf), "%.2f", comp.lightRadius);
        snprintf(compLightInnerConeBuf, sizeof(compLightInnerConeBuf), "%.2f", comp.lightInnerConeAngle);
        snprintf(compLightOuterConeBuf, sizeof(compLightOuterConeBuf), "%.2f", comp.lightOuterConeAngle);
        snprintf(compLightWidthBuf, sizeof(compLightWidthBuf), "%.2f", comp.lightWidth);
        snprintf(compLightHeightBuf, sizeof(compLightHeightBuf), "%.2f", comp.lightHeight);
        lastComponentDetailIndex = selectedComponentIndex;
        componentActiveField = 0;
        componentMeshPicker.Close();
        componentMaterialPicker.Close();
        audioPathPicker.Close();
        savePicker.Close();
        materialSlotPickerIndex = -1;
        inputMappingPicker.Close();
        inputKeyPicker.Close();
        inputDevicePicker.Close();
        inputKeyPickerMappingIndex = -1;
        inputKeyPickerBindingIndex = -1;
        listeningForInputBindingMappingIndex = -1;
        listeningForInputBindingIndex = -1;
    }

    auto saveButton = [&](Rectangle fieldRect, const char* fieldKey) {
        Rectangle btn = {fieldRect.x + fieldRect.width + 4, fieldRect.y, 18, 18};
        bool has = comp.savableFields.count(fieldKey) && !comp.savableFields[fieldKey].empty();
        DrawRectangleRec(btn, has ? Color{80, 140, 95, 255} : Color{38, 38, 42, 255});
        DrawRectangleLinesEx(btn, 1, {70, 70, 80, 255});
        DrawText("S", (int)btn.x + 5, (int)btn.y + 3, 11, has ? WHITE : Color{120, 120, 130, 255});
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, btn) && !pickerBlocksInput) {
            saveTargetFieldKey = fieldKey;
            savePicker.Open({btn.x, btn.y + btn.height}, SaveFileRegistry::Category());
        }
    };

    auto textField = [&](const char* label, char* buf, size_t bufSize, int fieldId, std::string& outVal,
                          const char* saveKey = nullptr) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        float w = panel.width - 16 - (saveKey ? kSaveColWidth : 0.0f);
        Rectangle r = {panel.x + 8, y, w, 18};
        if (PropertyField::StringBox(r, buf, bufSize, fieldId, componentActiveField, outVal, !pickerBlocksInput))
            isDirty = true;
        if (saveKey) saveButton(r, saveKey);
        y += 22;
    };

    auto floatField = [&](const char* label, char* buf, size_t bufSize, int fieldId, float& outVal,
                           const char* saveKey = nullptr) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        float w = panel.width - 16 - (saveKey ? kSaveColWidth : 0.0f);
        Rectangle r = {panel.x + 8, y, w, 18};
        if (PropertyField::FloatBox(r, buf, bufSize, fieldId, componentActiveField, outVal, !pickerBlocksInput))
            isDirty = true;
        if (saveKey) saveButton(r, saveKey);
        y += 22;
    };

    auto vec3Row = [&](const char* label, char bufs[3][32], int fieldBase, float& vx, float& vy, float& vz,
                        const char* saveKey = nullptr) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {180, 200, 230, 255});
        y += 16;
        float w = panel.width - 16 - (saveKey ? kSaveColWidth : 0.0f);
        Rectangle row = {panel.x + 8, y, w, 18};
        if (PropertyField::Vec3Row(row, bufs[0], bufs[1], bufs[2], fieldBase, componentActiveField,
                                    vx, vy, vz, !pickerBlocksInput))
            isDirty = true;
        if (saveKey) saveButton(row, saveKey);
        y += 22;
    };

    auto assetField = [&](const char* label, const std::string& current, AssetPickerPopup& picker,
                           const char* extension, std::string& outVal, const char* saveKey = nullptr) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        float w = panel.width - 16 - (saveKey ? kSaveColWidth : 0.0f);
        Rectangle r = {panel.x + 8, y, w, 18};
        DrawRectangleRec(r, {38, 38, 42, 255});
        DrawRectangleLinesEx(r, 1, {70, 70, 80, 255});
        std::string label2 = current.empty() ? "None" : std::filesystem::path(current).stem().string();
        DrawText(label2.c_str(), (int)r.x + 4, (int)r.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !pickerBlocksInput) {
            componentMeshPicker.Close();
            componentMaterialPicker.Close();
            picker.Open({r.x, r.y + r.height}, extension);
        }
        if (saveKey) saveButton(r, saveKey);
        y += 22;
        std::string picked;
        if (picker.Render(picked)) {
            outVal = picked;
            isDirty = true;
        }
    };

    DrawText(comp.componentType.c_str(), (int)panel.x + 8, (int)y, 13, WHITE);
    y += 20;

    textField("Name", compNameBuf, sizeof(compNameBuf), 1, comp.name);
    y += 4;

    {
        DrawText("Parent", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        std::vector<std::string> parentOptions = GetValidParentOptions(graph, (size_t)selectedComponentIndex);
        std::string parentLabel = comp.parentName.empty() ? "None (Root)" : comp.parentName;
        if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, parentLabel.c_str())) {
            auto it = std::find(parentOptions.begin(), parentOptions.end(), comp.parentName);
            size_t idx = (it != parentOptions.end()) ? (size_t)(it - parentOptions.begin()) : 0;
            idx = (idx + 1) % parentOptions.size();
            comp.parentName = parentOptions[idx];
            isDirty = true;
        }
        y += 24;
    }

    vec3Row("Position", compPosBuf, 2, comp.posX, comp.posY, comp.posZ, "Position");
    y += 2;
    vec3Row("Rotation", compRotBuf, 5, comp.rotX, comp.rotY, comp.rotZ, "Rotation");
    y += 2;
    vec3Row("Scale", compScaleBuf, 8, comp.scaleX, comp.scaleY, comp.scaleZ, "Scale");
    y += 6;

    if (comp.componentType == "MeshComponent") {
        assetField("Static Mesh", comp.meshAssetPath, componentMeshPicker, ".starasset", comp.meshAssetPath, "Static Mesh");

        std::vector<MaterialSlot> meshSlots;
        if (!comp.meshAssetPath.empty()) {
            StarAsset meshAsset;
            if (StarAsset::LoadFromFile(comp.meshAssetPath, meshAsset)) meshSlots = meshAsset.materialSlots;
        }
        if (comp.materialAssetPaths.size() != meshSlots.size()) comp.materialAssetPaths.resize(meshSlots.size());

        if (meshSlots.empty()) {
            DrawText("No material slots", (int)panel.x + 8, (int)y, 10, {130, 130, 140, 255});
            y += 18;
        } else {
            DrawText("Materials", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
            y += 14;
            for (size_t i = 0; i < meshSlots.size(); i++) {
                Rectangle rowRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
                DrawRectangleRec(rowRect, {38, 38, 42, 255});
                DrawRectangleLinesEx(rowRect, 1, {70, 70, 80, 255});
                std::string slotName = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
                DrawText(slotName.c_str(), (int)rowRect.x + 4, (int)rowRect.y + 2, 10, {150, 150, 160, 255});
                std::string matLabel = comp.materialAssetPaths[i].empty() ? "None" :
                    std::filesystem::path(comp.materialAssetPaths[i]).stem().string();
                DrawText(matLabel.c_str(), (int)(rowRect.x + rowRect.width * 0.5f), (int)rowRect.y + 2, 10, WHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rowRect) && !pickerBlocksInput) {
                    componentMeshPicker.Close();
                    componentMaterialPicker.Open({rowRect.x, rowRect.y + rowRect.height}, ".starmat");
                    materialSlotPickerIndex = (int)i;
                }
                y += PropertyField::ArrayRowHeight() + 3;
            }
            std::string pickedMat;
            if (componentMaterialPicker.Render(pickedMat)) {
                if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)comp.materialAssetPaths.size())
                    comp.materialAssetPaths[materialSlotPickerIndex] = pickedMat;
                isDirty = true;
            }
        }
    } else if (comp.componentType == "CollisionComponent") {
        DrawText("Shape", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, comp.collisionShape.c_str())) {
            if (comp.collisionShape == "Box") comp.collisionShape = "Sphere";
            else if (comp.collisionShape == "Sphere") comp.collisionShape = "Capsule";
            else comp.collisionShape = "Box";
            isDirty = true;
        }
        y += 24;
        if (comp.collisionShape != "Box") {
            floatField("Radius", compRadiusBuf, sizeof(compRadiusBuf), 11, comp.collisionRadius, "Radius");
        }
        if (comp.collisionShape == "Capsule") {
            floatField("Height", compHeightBuf, sizeof(compHeightBuf), 12, comp.collisionHeight, "Height");
        }
    } else if (comp.componentType == "AudioComponent") {
        DrawText("Sound Path", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        Rectangle r = {panel.x + 8, y, panel.width - 16 - kSaveColWidth, 18};
        DrawRectangleRec(r, {38, 38, 42, 255});
        DrawRectangleLinesEx(r, 1, {70, 70, 80, 255});
        std::string soundLabel = comp.soundAssetPath.empty() ? "None" : std::filesystem::path(comp.soundAssetPath).stem().string();
        DrawText(soundLabel.c_str(), (int)r.x + 4, (int)r.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !pickerBlocksInput) {
            audioPathPicker.Open({r.x, r.y + r.height}, ".starsound");
        }
        saveButton(r, "Sound Path");
        y += 22;
        std::string pickedSound;
        if (audioPathPicker.Render(pickedSound)) {
            comp.soundAssetPath = pickedSound;
            isDirty = true;
        }
        floatField("Volume", compVolumeBuf, sizeof(compVolumeBuf), 14, comp.volume, "Volume");
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Auto Play", &comp.autoPlay)) isDirty = true;
        saveButton({panel.x + 8, y, panel.width - 16 - kSaveColWidth, 18}, "Auto Play");
        y += 24;
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Looping", &comp.looping)) isDirty = true;
        saveButton({panel.x + 8, y, panel.width - 16 - kSaveColWidth, 18}, "Looping");
        y += 24;
    } else if (comp.componentType == "CameraComponent") {
        floatField("FOV", compFovBuf, sizeof(compFovBuf), 15, comp.cameraFov, "FOV");
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Orthographic", &comp.cameraOrthographic)) isDirty = true;
        saveButton({panel.x + 8, y, panel.width - 16 - kSaveColWidth, 18}, "Orthographic");
        y += 24;
    } else if (comp.componentType == "LightComponent") {
        DrawText("Light Type", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, comp.lightType.c_str())) {
            if (comp.lightType == "Point") comp.lightType = "Rectangle";
            else if (comp.lightType == "Rectangle") comp.lightType = "Spot";
            else if (comp.lightType == "Spot") comp.lightType = "Global";
            else if (comp.lightType == "Global") comp.lightType = "Sun";
            else comp.lightType = "Point";
            isDirty = true;
        }
        y += 24;

        vec3Row("Light Color", compLightColorBuf, 16, comp.lightColorR, comp.lightColorG, comp.lightColorB, "Light Color");
        y += 2;
        floatField("Intensity", compLightIntensityBuf, sizeof(compLightIntensityBuf), 19, comp.lightIntensity, "Intensity");

        if (comp.lightType != "Global" && comp.lightType != "Sun") {
            floatField("Radius", compLightRadiusBuf, sizeof(compLightRadiusBuf), 20, comp.lightRadius, "Radius");
        }
        if (comp.lightType == "Spot") {
            floatField("Inner Cone Angle", compLightInnerConeBuf, sizeof(compLightInnerConeBuf), 21, comp.lightInnerConeAngle, "Inner Cone Angle");
            floatField("Outer Cone Angle", compLightOuterConeBuf, sizeof(compLightOuterConeBuf), 22, comp.lightOuterConeAngle, "Outer Cone Angle");
        }
        if (comp.lightType == "Rectangle") {
            floatField("Width", compLightWidthBuf, sizeof(compLightWidthBuf), 23, comp.lightWidth, "Width");
            floatField("Height", compLightHeightBuf, sizeof(compLightHeightBuf), 24, comp.lightHeight, "Height");
        }
    } else if (comp.componentType == "InputComponent") {
        static const char* mappingTypeNames[] = {"Bool", "Axis", "Axis2D"};

        DrawText("Device", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        Rectangle deviceBtn = {panel.x + 60, y - 2, panel.width - 68, 16};
        DrawRectangleRec(deviceBtn, {50, 50, 58, 255});
        DrawText(comp.inputDevice.c_str(), (int)deviceBtn.x + 6, (int)deviceBtn.y + 3, 11, {200, 200, 210, 255});
        if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, deviceBtn)) {
            inputDevicePicker.Open({deviceBtn.x, deviceBtn.y + deviceBtn.height}, deviceBtn.width);
        }
        y += 22;

        DrawText("Mappings", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        Rectangle addMappingBtn = {panel.x + panel.width - 60, y - 2, 52, 16};
        if (GuiButton(addMappingBtn, "+ Add") && !pickerBlocksInput) {
            inputMappingPicker.Open({addMappingBtn.x, addMappingBtn.y + addMappingBtn.height}, InputMappingRegistry::Category());
        }
        y += 20;

        int scaleSlot = 0;
        int removeMappingIndex = -1;
        for (size_t mi = 0; mi < comp.inputMappingNames.size(); mi++) {
            const std::string& mappingName = comp.inputMappingNames[mi];
            InputMapping mapping;
            if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;

            Rectangle nameRow = {panel.x + 8, y, panel.width - 16, 18};
            DrawRectangleRec(nameRow, {38, 38, 42, 255});
            DrawText(mappingName.c_str(), (int)nameRow.x + 4, (int)nameRow.y + 3, 11, WHITE);

            Rectangle typeBtn = {nameRow.x + nameRow.width - 134, nameRow.y, 52, 18};
            DrawRectangleRec(typeBtn, {50, 50, 58, 255});
            DrawText(mappingTypeNames[(int)mapping.type], (int)typeBtn.x + 6, (int)typeBtn.y + 3, 11, {200, 200, 210, 255});
            if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, typeBtn)) {
                mapping.type = (InputMappingType)(((int)mapping.type + 1) % 3);
                InputMappingRegistry::Get().SetMapping(mappingName, mapping);
            }

            Rectangle addKeyBtn = {nameRow.x + nameRow.width - 78, nameRow.y, 42, 18};
            DrawRectangleRec(addKeyBtn, {50, 65, 50, 255});
            DrawText("+ Key", (int)addKeyBtn.x + 4, (int)addKeyBtn.y + 3, 10, WHITE);
            if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, addKeyBtn)) {
                inputKeyPickerMappingIndex = (int)mi;
                inputKeyPickerBindingIndex = -1;
                inputKeyPicker.Open({addKeyBtn.x, addKeyBtn.y + addKeyBtn.height});
            }

            Rectangle removeBtn = {nameRow.x + nameRow.width - 32, nameRow.y, 28, 18};
            DrawRectangleRec(removeBtn, {70, 45, 45, 255});
            DrawText("x", (int)removeBtn.x + 12, (int)removeBtn.y + 2, 12, WHITE);
            if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, removeBtn)) {
                removeMappingIndex = (int)mi;
            }
            y += 20;

            int removeBindingIndex = -1;
            for (size_t bi = 0; bi < mapping.bindings.size(); bi++) {
                InputMappingBinding& binding = mapping.bindings[bi];

                float rightColW = (mapping.type == InputMappingType::Bool) ? 46.0f :
                                   (mapping.type == InputMappingType::Axis2D) ? 118.0f : 90.0f;
                Rectangle keyRect = {panel.x + 16, y, panel.width - 16 - 8 - rightColW, 18};
                std::string display = binding.key;
                for (char& c : display) if (c == '_') c = ' ';
                DrawRectangleRec(keyRect, {35, 35, 40, 255});
                DrawRectangleLinesEx(keyRect, 1, {60, 60, 70, 255});
                DrawText(display.c_str(), (int)keyRect.x + 6, (int)keyRect.y + 3, 11, WHITE);
                if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, keyRect)) {
                    inputKeyPickerMappingIndex = (int)mi;
                    inputKeyPickerBindingIndex = (int)bi;
                    inputKeyPicker.Open({keyRect.x, keyRect.y + keyRect.height});
                }

                float x = keyRect.x + keyRect.width + 4;

                if (mapping.type != InputMappingType::Bool) {
                    Rectangle scaleRect = {x, y, 40, 18};
                    if (scaleSlot < 8) {
                        if (componentActiveField != 20 + scaleSlot)
                            snprintf(compInputScaleBuf[scaleSlot], 32, "%.2f", binding.scale);
                        bool committed = PropertyField::FloatBox(scaleRect, compInputScaleBuf[scaleSlot], 32, 20 + scaleSlot,
                                                                  componentActiveField, binding.scale, !pickerBlocksInput);
                        if (committed || componentActiveField == 20 + scaleSlot) {
                            InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                        }
                        scaleSlot++;
                    }
                    x += 44;
                }

                if (mapping.type == InputMappingType::Axis2D) {
                    Rectangle axisBtn = {x, y, 24, 18};
                    DrawRectangleRec(axisBtn, {50, 50, 58, 255});
                    DrawText(binding.axis == 0 ? "X" : "Y", (int)axisBtn.x + 8, (int)axisBtn.y + 3, 11, WHITE);
                    if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, axisBtn)) {
                        binding.axis = (binding.axis == 0) ? 1 : 0;
                        InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                    }
                    x += 28;
                }

                Rectangle listenBtn = {x, y, 22, 18};
                bool listening = (listeningForInputBindingMappingIndex == (int)mi && listeningForInputBindingIndex == (int)bi);
                DrawRectangleRec(listenBtn, listening ? Color{160, 90, 30, 255} : Color{50, 50, 56, 255});
                DrawText(listening ? "?" : "...", (int)listenBtn.x + 5, (int)listenBtn.y + 3, 10, WHITE);
                if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, listenBtn)) {
                    listeningForInputBindingMappingIndex = (int)mi;
                    listeningForInputBindingIndex = (int)bi;
                    inputKeyPicker.Close();
                }
                x += 26;

                Rectangle removeBindingBtn = {x, y, 18, 18};
                DrawRectangleRec(removeBindingBtn, {70, 45, 45, 255});
                DrawText("x", (int)removeBindingBtn.x + 6, (int)removeBindingBtn.y + 2, 11, WHITE);
                if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, removeBindingBtn)) {
                    removeBindingIndex = (int)bi;
                }

                y += 20;
            }

            if (removeBindingIndex != -1) {
                mapping.bindings.erase(mapping.bindings.begin() + removeBindingIndex);
                InputMappingRegistry::Get().SetMapping(mappingName, mapping);
            }

            y += 4;
        }

        if (removeMappingIndex != -1) {
            comp.inputMappingNames.erase(comp.inputMappingNames.begin() + removeMappingIndex);
            isDirty = true;
        }
    }

    // Device picker
    if (comp.componentType == "InputComponent" && inputDevicePicker.IsOpen()) {
        std::vector<SearchableListPopup::Entry> deviceEntries;
        deviceEntries.push_back({"Devices", "Unclaimed", "Unclaimed"});
        deviceEntries.push_back({"Devices", "Keyboard+Mouse", "Keyboard+Mouse"});
        for (int i = 0; i < 4; i++) {
            if (!IsGamepadAvailable(i)) continue;
            std::string name = "Gamepad " + std::to_string(i);
            deviceEntries.push_back({"Devices", name, name});
        }

        std::string pickedDevice = inputDevicePicker.Render(deviceEntries, panel);
        if (!pickedDevice.empty() && pickedDevice != comp.inputDevice) {
            comp.inputDevice = pickedDevice;
            isDirty = true;
        }
    }

    // Mapping name picker
    std::string pickedMappingName;
    if (comp.componentType == "InputComponent" && inputMappingPicker.Render(pickedMappingName) && !pickedMappingName.empty()) {
        bool alreadyOwned = std::find(comp.inputMappingNames.begin(), comp.inputMappingNames.end(), pickedMappingName) != comp.inputMappingNames.end();
        if (!alreadyOwned) {
            if (!InputMappingRegistry::Get().Find(pickedMappingName)) InputMappingRegistry::Get().SetMapping(pickedMappingName, InputMapping{});
            comp.inputMappingNames.push_back(pickedMappingName);
            isDirty = true;
        }
    }

    // Key picker for a binding row
    if (comp.componentType == "InputComponent" && inputKeyPicker.IsOpen()) {
        std::vector<SearchableListPopup::Entry> keyEntries;
        for (const std::string& keyName : GetInputKeyNames()) {
            if (InputKeyIsAnalog(keyName)) continue;
            std::string category = "Keyboard";
            if (keyName.rfind("GAMEPAD_", 0) == 0) category = "Gamepad";
            else if (keyName.find("MOUSE") != std::string::npos) category = "Mouse";
            std::string label = keyName;
            for (char& c : label) if (c == '_') c = ' ';
            keyEntries.push_back({category, keyName, label});
        }

        std::string pickedKey = inputKeyPicker.Render(keyEntries, panel);
        if (!pickedKey.empty() && inputKeyPickerMappingIndex >= 0 && inputKeyPickerMappingIndex < (int)comp.inputMappingNames.size()) {
            const std::string& mappingName = comp.inputMappingNames[inputKeyPickerMappingIndex];
            InputMapping mapping;
            if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;
            if (inputKeyPickerBindingIndex >= 0 && inputKeyPickerBindingIndex < (int)mapping.bindings.size()) {
                mapping.bindings[inputKeyPickerBindingIndex].key = pickedKey;
            } else {
                InputMappingBinding newBinding;
                newBinding.key = pickedKey;
                mapping.bindings.push_back(newBinding);
            }
            InputMappingRegistry::Get().SetMapping(mappingName, mapping);
        }
    }

    // Listening for a keyboard press to bind directly, same keyboard-only capture as Input_Key's listen button.
    if (listeningForInputBindingMappingIndex != -1) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            listeningForInputBindingMappingIndex = -1;
            listeningForInputBindingIndex = -1;
        } else {
            int keyCode = GetKeyPressed();
            if (keyCode != 0) {
                std::string keyName = InputKeyCodeToName(keyCode);
                if (!keyName.empty() && listeningForInputBindingMappingIndex < (int)comp.inputMappingNames.size()) {
                    const std::string& mappingName = comp.inputMappingNames[listeningForInputBindingMappingIndex];
                    InputMapping mapping;
                    if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;
                    if (listeningForInputBindingIndex >= 0 && listeningForInputBindingIndex < (int)mapping.bindings.size()) {
                        mapping.bindings[listeningForInputBindingIndex].key = keyName;
                        InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                    }
                }
                listeningForInputBindingMappingIndex = -1;
                listeningForInputBindingIndex = -1;
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                listeningForInputBindingMappingIndex = -1;
                listeningForInputBindingIndex = -1;
            }
        }
    }

    std::string pickedSaveFile;
    if (savePicker.Render(pickedSaveFile)) {
        if (pickedSaveFile.empty()) comp.savableFields.erase(saveTargetFieldKey);
        else comp.savableFields[saveTargetFieldKey] = pickedSaveFile;
        isDirty = true;
    }
}

static const char* ParamTypeShortName(PinType type) {
    switch (type) {
        case PinType::Boolean: return "Bool";
        case PinType::Integer: return "Int";
        case PinType::Float:   return "Float";
        case PinType::String:  return "String";
        case PinType::Vec3:    return "Vec3";
        case PinType::Object:  return "Object";
        default:                return "Float";
    }
}

static PinType NextParamType(PinType type) {
    switch (type) {
        case PinType::Boolean: return PinType::Integer;
        case PinType::Integer: return PinType::Float;
        case PinType::Float:   return PinType::String;
        case PinType::String:  return PinType::Vec3;
        case PinType::Vec3:    return PinType::Object;
        default:                return PinType::Boolean;
    }
}

void StargraphEditorPanel::RenderCustomEventDetails(Rectangle panel, float y, Node& node) {
    int nodeId = std::stoi(node.id);
    if (lastEventDetailNodeId != nodeId) {
        snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", node.title.c_str());
        int row = 0;
        for (const Pin& pin : node.outputs) {
            if (pin.type == PinType::Exec) continue;
            if (row < 8) snprintf(eventParamNameBuf[row], 64, "%s", pin.name.c_str());
            row++;
        }
        lastEventDetailNodeId = nodeId;
        eventActiveField = 0;
    }

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;
    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    std::string titleVal = node.title;
    if (PropertyField::StringBox(nameRect, eventNameBuf, sizeof(eventNameBuf), 1, eventActiveField, titleVal)) {
        node.title = titleVal;
        isDirty = true;
    }
    y += 26;

    DrawText("Replication", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;
    static const char* replNames[] = {"Local", "Replicated", "Server Only"};
    if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, replNames[(int)node.eventReplication])) {
        node.eventReplication = (ReplicationMode)(((int)node.eventReplication + 1) % 3);
        isDirty = true;
    }
    y += 24;

    if (node.eventReplication != ReplicationMode::Local) {
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Reliable", &node.eventReliable)) isDirty = true;
        y += 24;
    }

    y += 6;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
    y += 10;
    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
    y += 18;

    std::string pinToRemove;
    int row = 0;
    for (Pin& pin : node.outputs) {
        if (pin.type == PinType::Exec) continue;

        if (row < 8) {
            Rectangle nameR = {panel.x + 8, y, panel.width - 16 - 66, 18};
            std::string pname = pin.name;
            if (PropertyField::StringBox(nameR, eventParamNameBuf[row], 64, 10 + row, eventActiveField, pname)) {
                pin.name = pname;
                isDirty = true;
            }
        }

        Rectangle typeR = {panel.x + panel.width - 54, y, 30, 18};
        if (GuiButton(typeR, ParamTypeShortName(pin.type))) {
            pin.type = NextParamType(pin.type);
            isDirty = true;
        }

        Rectangle delR = {panel.x + panel.width - 20, y, 18, 18};
        if (GuiButton(delR, "X")) pinToRemove = pin.name;

        y += 22;
        row++;
    }

    if (!pinToRemove.empty()) {
        graph.RemoveDynamicPin(node, true, pinToRemove);
        isDirty = true;
    }

    y += 6;
    if (row < 8) {
        if (GuiButton({panel.x + 8, y, panel.width - 16, 20}, "+ Add Parameter")) {
            std::string newName;
            int n = 1;
            do {
                newName = "Param" + std::to_string(n++);
                bool taken = false;
                for (const Pin& p : node.outputs) if (p.name == newName) { taken = true; break; }
                if (!taken) break;
            } while (n < 100);
            graph.AddDynamicPin(node, true, newName, PinType::Float);
            isDirty = true;
        }
    }
}

void StargraphEditorPanel::RenderCallCustomEventDetails(Rectangle panel, float y, Node& node) {
    int nodeId = std::stoi(node.id);
    if (lastEventDetailNodeId != nodeId) {
        eventNameBuf[0] = '\0';
        for (const Pin& p : node.inputs) if (p.name == "Event Name") snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", p.defaultValue.c_str());
        lastEventDetailNodeId = nodeId;
        eventActiveField = 0;
    }

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;

    Pin* eventNamePin = nullptr;
    for (Pin& p : node.inputs) if (p.name == "Event Name") eventNamePin = &p;

    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    if (eventNamePin) {
        std::string val = eventNamePin->defaultValue;
        if (PropertyField::StringBox(nameRect, eventNameBuf, sizeof(eventNameBuf), 1, eventActiveField, val)) {
            eventNamePin->defaultValue = val;
            isDirty = true;
        }
    }
    y += 26;

    if (GuiButton({panel.x + 8, y, panel.width - 16, 20}, "Refresh Pins")) {
        std::string targetName = eventNamePin ? std::string(eventNamePin->defaultValue) : std::string();
        Node* targetEvent = graph.FindCustomEventByName(targetName);
        if (targetEvent) {
            std::vector<std::string> keep = {"Target", "Event Name"};
            for (const Pin& srcPin : targetEvent->outputs) {
                if (srcPin.type == PinType::Exec) continue;
                graph.AddDynamicPin(node, false, srcPin.name, srcPin.type);
                keep.push_back(srcPin.name);
            }
            std::vector<std::string> toRemove;
            for (const Pin& p : node.inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keep) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemove.push_back(p.name);
            }
            for (const std::string& n : toRemove) graph.RemoveDynamicPin(node, false, n);
            isDirty = true;
        }
    }
    y += 28;

    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
    y += 18;
    for (const Pin& pin : node.inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Target" || pin.name == "Event Name") continue;
        DrawText((pin.name + " (" + ParamTypeShortName(pin.type) + ")").c_str(), (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 16;
    }
}