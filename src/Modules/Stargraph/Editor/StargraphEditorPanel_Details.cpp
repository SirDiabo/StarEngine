#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Editor/PropertyFields.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/StarAsset.h"
#include "Core/SaveFileRegistry.h"
#include "Core/SafeParse.h"
#include "Editor/UITheme.h"
#include "Editor/UIWidgets.h"
#include "raygui.h"
#include <algorithm>
#include <cstring>
#include <filesystem>

void StargraphEditorPanel::RenderComponentOutliner(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(area);

    Vector2 mousePos = GetMousePosition();
    float y = area.y + 6;
    bool popupWasOpenThisFrame = addComponentMenu.IsOpen();
    bool leftClickConsumedByComponent = false;

    DrawText("Components", (int)area.x + 8, (int)y, 12, theme.textSecondary);

    // Add Component button.
    Rectangle addBtn = {area.x + area.width - 50, y - 2, 44, 16};
    if (UIWidget::Button(addBtn, "+ Add")) {
        if (addComponentMenu.IsOpen()) addComponentMenu.Close();
        else addComponentMenu.Open({addBtn.x, addBtn.y + addBtn.height});
    }

    y += 18;
    DrawLine((int)area.x + 4, (int)y, (int)(area.x + area.width - 4), (int)y, theme.panelBorder);
    y += 6;

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    float listBottom = area.y + area.height - 4;

    for (size_t i = 0; i < comps.size() && y + 18 <= listBottom; i++) {
        Rectangle row = {area.x + 4, y, area.width - 8, 18};
        bool isSelected = (selectedComponentIndex == (int)i);

        DrawRectangleRec(row, isSelected ? theme.surfaceSelected : theme.fieldBg);
        DrawRectangleRec({area.x + 8, y + 4, 10, 10}, {90, 150, 210, 255});

        std::string label = comps[i].name + " (" + comps[i].componentType + ")";
        DrawText(label.c_str(), (int)area.x + 24, (int)y + 2, 11, theme.textPrimary);

        if (CheckCollisionPointRec(mousePos, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            leftClickConsumedByComponent = true;
            selectedComponentIndex = (int)i;
            lastValidComponentIndex = (int)i;
            selectedNodeId = -1;
            selectedNodeIds.clear();
            selectedVariableIndex = -1;
            lastValidVariableIndex = -1;
            selectedFunctionIndex = -1;
            lastValidFunctionIndex = -1;
            selectedLocalVariableIndex = -1;
            lastValidLocalVariableIndex = -1;
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
            lastValidComponentIndex = -1;
            isDirty = true;
            break;
        }

        y += 20;
    }

    if (comps.empty()) {
        DrawText("No components", (int)area.x + 8, (int)y, 11, theme.textSecondary);
    }

    if (addComponentMenu.IsOpen()) {
        std::vector<SearchableListPopup::Entry> entries;
        entries.push_back({"Components", "SceneComponent", "SceneComponent"});
        for (const ComponentTypeInfo& typeInfo : ComponentRegistry::Get().GetAll()) {
            entries.push_back({"Components", typeInfo.componentType, typeInfo.componentType});
        }

        std::string picked = addComponentMenu.Render(entries, screenBounds);
        if (!picked.empty()) {
            ComponentDecl comp;
            comp.componentType = picked;
            comp.name          = picked;
            graph.AddComponent(comp);
            selectedComponentIndex = (int)graph.GetComponents().size() - 1;
            lastValidComponentIndex = selectedComponentIndex;
            selectedNodeId = -1;
            selectedNodeIds.clear();
            selectedVariableIndex = -1;
            lastValidVariableIndex = -1;
            selectedFunctionIndex = -1;
            lastValidFunctionIndex = -1;
            selectedLocalVariableIndex = -1;
            lastValidLocalVariableIndex = -1;
            isDirty = true;
        }
    }

    if (!popupWasOpenThisFrame && !leftClickConsumedByComponent && selectedComponentIndex != -1 &&
        previewGizmoState.hoverAxis == -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selectedComponentIndex = -1;
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
    const UIPalette& theme = UITheme::Get();
    ComponentDecl& comp = graph.GetComponents()[lastValidComponentIndex];
    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = componentMaterialPicker.ContainsPoint(mp) || savePicker.ContainsPoint(mp)
                            || inputMappingPicker.ContainsPoint(mp) || inputKeyPicker.ContainsPoint(mp)
                            || inputDevicePicker.ContainsPoint(mp);
    for (auto& kv : genericAssetPickers) {
        if (kv.second.ContainsPoint(mp)) { pickerBlocksInput = true; break; }
    }
    const float kSaveColWidth = 24.0f;

    if (lastComponentDetailIndex != lastValidComponentIndex) {
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
        lastComponentDetailIndex = lastValidComponentIndex;
        componentActiveField = 0;
        genericFieldBufs.clear();
        genericAssetPickers.clear();
        activeDeviceFieldKey.clear();
        componentMaterialPicker.Close();
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

    auto refreshField = [&](char* buf, size_t bufSize, int fieldId, float val) {
        if (componentActiveField == fieldId) return;
        snprintf(buf, bufSize, "%.2f", val);
    };
    refreshField(compPosBuf[0], 32, 2, comp.posX);
    refreshField(compPosBuf[1], 32, 3, comp.posY);
    refreshField(compPosBuf[2], 32, 4, comp.posZ);
    refreshField(compRotBuf[0], 32, 5, comp.rotX);
    refreshField(compRotBuf[1], 32, 6, comp.rotY);
    refreshField(compRotBuf[2], 32, 7, comp.rotZ);
    refreshField(compScaleBuf[0], 32, 8, comp.scaleX);
    refreshField(compScaleBuf[1], 32, 9, comp.scaleY);
    refreshField(compScaleBuf[2], 32, 10, comp.scaleZ);

    auto saveButton = [&](Rectangle fieldRect, const char* fieldKey) {
        Rectangle btn = {fieldRect.x + fieldRect.width + 4, fieldRect.y, 18, 18};
        bool has = comp.savableFields.count(fieldKey) && !comp.savableFields[fieldKey].empty();
        DrawRectangleRec(btn, has ? theme.success : theme.fieldBg);
        DrawRectangleLinesEx(btn, 1, theme.fieldBorder);
        DrawText("S", (int)btn.x + 5, (int)btn.y + 3, 11, has ? theme.textPrimary : theme.buttonDisabledText);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, btn) && !pickerBlocksInput) {
            saveTargetFieldKey = fieldKey;
            savePicker.Open({btn.x, btn.y + btn.height}, SaveFileRegistry::Category());
        }
    };

    auto textField = [&](const char* label, char* buf, size_t bufSize, int fieldId, std::string& outVal,
                          const char* saveKey = nullptr) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, theme.textSecondary);
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
        DrawText(label, (int)panel.x + 8, (int)y, 11, theme.textSecondary);
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
        DrawText(label, (int)panel.x + 8, (int)y, 11, theme.sectionHeader);
        y += 16;
        float w = panel.width - 16 - (saveKey ? kSaveColWidth : 0.0f);
        Rectangle row = {panel.x + 8, y, w, 18};
        if (PropertyField::Vec3Row(row, bufs[0], bufs[1], bufs[2], fieldBase, componentActiveField,
                                    vx, vy, vz, !pickerBlocksInput))
            isDirty = true;
        if (saveKey) saveButton(row, saveKey);
        y += 22;
    };

    DrawText(comp.componentType.c_str(), (int)panel.x + 8, (int)y, 13, theme.textPrimary);
    y += 20;

    textField("Name", compNameBuf, sizeof(compNameBuf), 1, comp.name);
    y += 4;

    {
        DrawText("Parent", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 14;
        std::vector<std::string> parentOptions = GetValidParentOptions(graph, (size_t)lastValidComponentIndex);
        std::string parentLabel = comp.parentName.empty() ? "None (Root)" : comp.parentName;
        if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 18}, parentLabel.c_str())) {
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

    const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(comp.componentType);
    if (typeInfo) {
        int fieldId = 50;
        for (const ComponentFieldDesc& f : typeInfo->fields) {
            fieldId++;
            if (f.visibleIf && !f.visibleIf(comp)) continue;
            switch (f.kind) {
                case ComponentFieldKind::String: {
                    std::string value = comp.Get(f.key, f.defaultValue);
                    auto& buf = genericFieldBufs[f.key];
                    if (componentActiveField != fieldId) snprintf(buf.data(), buf.size(), "%s", value.c_str());
                    DrawText(f.label.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
                    y += 14;
                    Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
                    if (PropertyField::StringBox(r, buf.data(), buf.size(), fieldId, componentActiveField, value, !pickerBlocksInput)) {
                        comp.fields[f.key] = value;
                        isDirty = true;
                    }
                    y += 22;
                    break;
                }
                case ComponentFieldKind::Float: {
                    float value = SafeParse::ToFloat(comp.Get(f.key, f.defaultValue));
                    auto& buf = genericFieldBufs[f.key];
                    if (componentActiveField != fieldId) snprintf(buf.data(), buf.size(), "%.2f", value);
                    DrawText(f.label.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
                    y += 14;
                    Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
                    if (PropertyField::FloatBox(r, buf.data(), buf.size(), fieldId, componentActiveField, value, !pickerBlocksInput)) {
                        char vbuf[32];
                        snprintf(vbuf, sizeof(vbuf), "%g", value);
                        comp.fields[f.key] = vbuf;
                        isDirty = true;
                    }
                    y += 22;
                    break;
                }
                case ComponentFieldKind::Bool: {
                    bool value = comp.Get(f.key, f.defaultValue) == "true";
                    bool before = value;
                    GuiCheckBox({panel.x + 8, y, 18, 18}, f.label.c_str(), &value);
                    if (value != before) {
                        comp.fields[f.key] = value ? "true" : "false";
                        isDirty = true;
                    }
                    y += 24;
                    break;
                }
                case ComponentFieldKind::Enum: {
                    std::string value = comp.Get(f.key, f.defaultValue);
                    DrawText(f.label.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
                    y += 14;
                    if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 18}, value.c_str()) && !f.enumOptions.empty()) {
                        auto optIt = std::find(f.enumOptions.begin(), f.enumOptions.end(), value);
                        size_t idx = (optIt != f.enumOptions.end()) ? (size_t)(optIt - f.enumOptions.begin()) : 0;
                        idx = (idx + 1) % f.enumOptions.size();
                        comp.fields[f.key] = f.enumOptions[idx];
                        isDirty = true;
                    }
                    y += 24;
                    break;
                }
                case ComponentFieldKind::AssetPath: {
                    std::string value = comp.Get(f.key, f.defaultValue);
                    AssetPickerPopup& picker = genericAssetPickers[f.key];
                    DrawText(f.label.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
                    y += 14;
                    Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
                    DrawRectangleRec(r, theme.fieldBg);
                    DrawRectangleLinesEx(r, 1, theme.fieldBorder);
                    std::string shortLabel = value.empty() ? "None" : std::filesystem::path(value).stem().string();
                    DrawText(shortLabel.c_str(), (int)r.x + 4, (int)r.y + 3, 11, theme.textPrimary);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !pickerBlocksInput) {
                        picker.Open({r.x, r.y + r.height}, f.assetExtension);
                    }
                    y += 22;
                    std::string picked;
                    if (picker.Render(picked)) {
                        comp.fields[f.key] = picked;
                        isDirty = true;
                    }
                    break;
                }
                case ComponentFieldKind::Device: {
                    std::string value = comp.Get(f.key, f.defaultValue);
                    DrawText(f.label.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
                    Rectangle r = {panel.x + 60, y - 2, panel.width - 68, 16};
                    DrawRectangleRec(r, theme.buttonBg);
                    DrawText(value.c_str(), (int)r.x + 6, (int)r.y + 3, 11, theme.textPrimary);
                    if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r)) {
                        activeDeviceFieldKey = f.key;
                        inputDevicePicker.Open({r.x, r.y + r.height}, r.width);
                    }
                    y += 22;
                    break;
                }
            }
        }
    }

    if (comp.componentType == "MeshComponent") {
        std::vector<MaterialSlot> meshSlots;
        std::string meshPath = comp.Get("Mesh");
        if (!meshPath.empty()) {
            StarAsset meshAsset;
            if (StarAsset::LoadFromFile(meshPath, meshAsset)) meshSlots = meshAsset.materialSlots;
        }
        std::vector<std::string>& materialAssetPaths = comp.arrayFields["Material"];
        if (materialAssetPaths.size() != meshSlots.size()) materialAssetPaths.resize(meshSlots.size());

        if (meshSlots.empty()) {
            DrawText("No material slots", (int)panel.x + 8, (int)y, 10, theme.textSecondary);
            y += 18;
        } else {
            DrawText("Materials", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
            y += 14;
            for (size_t i = 0; i < meshSlots.size(); i++) {
                Rectangle rowRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
                DrawRectangleRec(rowRect, theme.fieldBg);
                DrawRectangleLinesEx(rowRect, 1, theme.fieldBorder);
                std::string slotName = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
                DrawText(slotName.c_str(), (int)rowRect.x + 4, (int)rowRect.y + 2, 10, theme.textSecondary);
                std::string matLabel = materialAssetPaths[i].empty() ? "None" :
                    std::filesystem::path(materialAssetPaths[i]).stem().string();
                DrawText(matLabel.c_str(), (int)(rowRect.x + rowRect.width * 0.5f), (int)rowRect.y + 2, 10, theme.textPrimary);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rowRect) && !pickerBlocksInput) {
                    componentMaterialPicker.Open({rowRect.x, rowRect.y + rowRect.height}, ".starmat");
                    materialSlotPickerIndex = (int)i;
                }
                y += PropertyField::ArrayRowHeight() + 3;
            }
            std::string pickedMat;
            if (componentMaterialPicker.Render(pickedMat)) {
                if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)materialAssetPaths.size())
                    materialAssetPaths[materialSlotPickerIndex] = pickedMat;
                isDirty = true;
            }
        }
    } else if (comp.componentType == "InputComponent") {
        static const char* mappingTypeNames[] = {"Bool", "Axis", "Axis2D"};
        std::vector<std::string>& inputMappingNames = comp.arrayFields["InputMapping"];

        DrawText("Mappings", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        Rectangle addMappingBtn = {panel.x + panel.width - 60, y - 2, 52, 16};
        if (UIWidget::Button(addMappingBtn, "+ Add") && !pickerBlocksInput) {
            inputMappingPicker.Open({addMappingBtn.x, addMappingBtn.y + addMappingBtn.height}, InputMappingRegistry::Category());
        }
        y += 20;

        int scaleSlot = 0;
        int removeMappingIndex = -1;
        for (size_t mi = 0; mi < inputMappingNames.size(); mi++) {
            const std::string& mappingName = inputMappingNames[mi];
            InputMapping mapping;
            if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;

            Rectangle nameRow = {panel.x + 8, y, panel.width - 16, 18};
            DrawRectangleRec(nameRow, theme.fieldBg);
            DrawText(mappingName.c_str(), (int)nameRow.x + 4, (int)nameRow.y + 3, 11, theme.textPrimary);

            Rectangle typeBtn = {nameRow.x + nameRow.width - 134, nameRow.y, 52, 18};
            DrawRectangleRec(typeBtn, theme.buttonBg);
            DrawText(mappingTypeNames[(int)mapping.type], (int)typeBtn.x + 6, (int)typeBtn.y + 3, 11, theme.textPrimary);
            if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, typeBtn)) {
                mapping.type = (InputMappingType)(((int)mapping.type + 1) % 3);
                InputMappingRegistry::Get().SetMapping(mappingName, mapping);
            }

            Rectangle addKeyBtn = {nameRow.x + nameRow.width - 78, nameRow.y, 42, 18};
            DrawRectangleRec(addKeyBtn, theme.successBg);
            DrawText("+ Key", (int)addKeyBtn.x + 4, (int)addKeyBtn.y + 3, 10, theme.textPrimary);
            if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, addKeyBtn)) {
                inputKeyPickerMappingIndex = (int)mi;
                inputKeyPickerBindingIndex = -1;
                inputKeyPicker.Open({addKeyBtn.x, addKeyBtn.y + addKeyBtn.height});
            }

            Rectangle removeBtn = {nameRow.x + nameRow.width - 32, nameRow.y, 28, 18};
            DrawRectangleRec(removeBtn, theme.dangerBg);
            DrawText("x", (int)removeBtn.x + 12, (int)removeBtn.y + 2, 12, theme.textPrimary);
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
                DrawRectangleRec(keyRect, theme.fieldBg);
                DrawRectangleLinesEx(keyRect, 1, theme.fieldBorder);
                DrawText(display.c_str(), (int)keyRect.x + 6, (int)keyRect.y + 3, 11, theme.textPrimary);
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
                    DrawRectangleRec(axisBtn, theme.buttonBg);
                    DrawText(binding.axis == 0 ? "X" : "Y", (int)axisBtn.x + 8, (int)axisBtn.y + 3, 11, theme.textPrimary);
                    if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, axisBtn)) {
                        binding.axis = (binding.axis == 0) ? 1 : 0;
                        InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                    }
                    x += 28;
                }

                Rectangle listenBtn = {x, y, 22, 18};
                bool listening = (listeningForInputBindingMappingIndex == (int)mi && listeningForInputBindingIndex == (int)bi);
                DrawRectangleRec(listenBtn, listening ? Color{160, 90, 30, 255} : theme.buttonBg);
                DrawText(listening ? "?" : "...", (int)listenBtn.x + 5, (int)listenBtn.y + 3, 10, theme.textPrimary);
                if (!pickerBlocksInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, listenBtn)) {
                    listeningForInputBindingMappingIndex = (int)mi;
                    listeningForInputBindingIndex = (int)bi;
                    inputKeyPicker.Close();
                }
                x += 26;

                Rectangle removeBindingBtn = {x, y, 18, 18};
                DrawRectangleRec(removeBindingBtn, theme.dangerBg);
                DrawText("x", (int)removeBindingBtn.x + 6, (int)removeBindingBtn.y + 2, 11, theme.textPrimary);
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
            inputMappingNames.erase(inputMappingNames.begin() + removeMappingIndex);
            isDirty = true;
        }
    }

    // Device picker
    if (!activeDeviceFieldKey.empty() && inputDevicePicker.IsOpen()) {
        std::vector<SearchableListPopup::Entry> deviceEntries;
        deviceEntries.push_back({"Devices", "Unclaimed", "Unclaimed"});
        deviceEntries.push_back({"Devices", "Keyboard+Mouse", "Keyboard+Mouse"});
        for (int i = 0; i < 4; i++) {
            if (!IsGamepadAvailable(i)) continue;
            std::string name = "Gamepad " + std::to_string(i);
            deviceEntries.push_back({"Devices", name, name});
        }

        std::string pickedDevice = inputDevicePicker.Render(deviceEntries, panel);
        if (!pickedDevice.empty()) {
            comp.fields[activeDeviceFieldKey] = pickedDevice;
            isDirty = true;
        }
    }

    // Mapping name picker
    std::string pickedMappingName;
    if (comp.componentType == "InputComponent" && inputMappingPicker.Render(pickedMappingName) && !pickedMappingName.empty()) {
        std::vector<std::string>& inputMappingNames = comp.arrayFields["InputMapping"];
        bool alreadyOwned = std::find(inputMappingNames.begin(), inputMappingNames.end(), pickedMappingName) != inputMappingNames.end();
        if (!alreadyOwned) {
            if (!InputMappingRegistry::Get().Find(pickedMappingName)) InputMappingRegistry::Get().SetMapping(pickedMappingName, InputMapping{});
            inputMappingNames.push_back(pickedMappingName);
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
        const std::vector<std::string>& inputMappingNames = comp.arrayFields["InputMapping"];
        if (!pickedKey.empty() && inputKeyPickerMappingIndex >= 0 && inputKeyPickerMappingIndex < (int)inputMappingNames.size()) {
            const std::string& mappingName = inputMappingNames[inputKeyPickerMappingIndex];
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
                const std::vector<std::string>& inputMappingNames = comp.arrayFields["InputMapping"];
                if (!keyName.empty() && listeningForInputBindingMappingIndex < (int)inputMappingNames.size()) {
                    const std::string& mappingName = inputMappingNames[listeningForInputBindingMappingIndex];
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
        case PinType::Rotator: return "Rotator";
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
        case PinType::Vec3:    return PinType::Rotator;
        case PinType::Rotator: return PinType::Object;
        default:                return PinType::Boolean;
    }
}

void StargraphEditorPanel::RenderCustomEventDetails(Rectangle panel, float y, Node& node) {
    const UIPalette& theme = UITheme::Get();
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

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    std::string titleVal = node.title;
    if (PropertyField::StringBox(nameRect, eventNameBuf, sizeof(eventNameBuf), 1, eventActiveField, titleVal)) {
        node.title = titleVal;
        isDirty = true;
    }
    y += 26;

    DrawText("Replication", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    static const char* replNames[] = {"Local", "Replicated", "Server Only"};
    if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 18}, replNames[(int)node.eventReplication])) {
        node.eventReplication = (ReplicationMode)(((int)node.eventReplication + 1) % 3);
        isDirty = true;
    }
    y += 24;

    if (node.eventReplication != ReplicationMode::Local) {
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Reliable", &node.eventReliable)) isDirty = true;
        y += 24;
    }

    y += 6;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, theme.panelBorder);
    y += 10;
    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
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
        if (UIWidget::Button(typeR, ParamTypeShortName(pin.type))) {
            pin.type = NextParamType(pin.type);
            isDirty = true;
        }

        Rectangle delR = {panel.x + panel.width - 20, y, 18, 18};
        if (UIWidget::Button(delR, "X")) pinToRemove = pin.name;

        y += 22;
        row++;
    }

    if (!pinToRemove.empty()) {
        graph.RemoveDynamicPin(node, true, pinToRemove);
        isDirty = true;
    }

    y += 6;
    if (row < 8) {
        if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 20}, "+ Add Parameter")) {
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
    const UIPalette& theme = UITheme::Get();
    int nodeId = std::stoi(node.id);
    if (lastEventDetailNodeId != nodeId) {
        eventNameBuf[0] = '\0';
        for (const Pin& p : node.inputs) if (p.name == "Event Name") snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", p.defaultValue.c_str());
        lastEventDetailNodeId = nodeId;
        eventActiveField = 0;
    }

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
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

    if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 20}, "Refresh Pins")) {
        std::string targetName = eventNamePin ? std::string(eventNamePin->defaultValue) : std::string();
        Node* targetEvent = graph.FindCustomEventByName(targetName);
        if (targetEvent) {
            std::vector<std::string> keep = {"Target", "Event Name"};
            for (const Pin& srcPin : targetEvent->outputs) {
                if (srcPin.type == PinType::Exec) continue;
                ActiveGraph().AddDynamicPin(node, false, srcPin.name, srcPin.type);
                keep.push_back(srcPin.name);
            }
            std::vector<std::string> toRemove;
            for (const Pin& p : node.inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keep) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemove.push_back(p.name);
            }
            for (const std::string& n : toRemove) ActiveGraph().RemoveDynamicPin(node, false, n);
            isDirty = true;
        }
    }
    y += 28;

    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
    y += 18;
    for (const Pin& pin : node.inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Target" || pin.name == "Event Name") continue;
        DrawText((pin.name + " (" + ParamTypeShortName(pin.type) + ")").c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 16;
    }
}

void StargraphEditorPanel::RenderCallFunctionDetails(Rectangle panel, float y, Node& node) {
    const UIPalette& theme = UITheme::Get();
    int nodeId = std::stoi(node.id);
    if (lastFuncCallDetailNodeId != nodeId) {
        funcCallNameBuf[0] = '\0';
        for (const Pin& p : node.inputs) if (p.name == "Function Name") snprintf(funcCallNameBuf, sizeof(funcCallNameBuf), "%s", p.defaultValue.c_str());
        lastFuncCallDetailNodeId = nodeId;
        funcCallActiveField = 0;
    }

    DrawText("Function Name", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;

    Pin* functionNamePin = nullptr;
    for (Pin& p : node.inputs) if (p.name == "Function Name") functionNamePin = &p;

    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    if (functionNamePin) {
        std::string val = functionNamePin->defaultValue;
        if (PropertyField::StringBox(nameRect, funcCallNameBuf, sizeof(funcCallNameBuf), 1, funcCallActiveField, val)) {
            functionNamePin->defaultValue = val;
            isDirty = true;
        }
    }
    y += 26;

    if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 20}, "Refresh Pins")) {
        std::string targetName = functionNamePin ? std::string(functionNamePin->defaultValue) : std::string();
        FunctionDef* targetFn = graph.FindFunction(targetName);
        if (targetFn) {
            Node* entryNode = targetFn->graph.FindNodeByType("Function_Entry");
            Node* returnNode = targetFn->graph.FindNodeByType("Return");

            std::vector<std::string> keepInputs = {"Function Name"};
            if (entryNode) {
                for (const Pin& srcPin : entryNode->outputs) {
                    if (srcPin.type == PinType::Exec) continue;
                    ActiveGraph().AddDynamicPin(node, false, srcPin.name, srcPin.type);
                    keepInputs.push_back(srcPin.name);
                }
            }
            std::vector<std::string> toRemoveIn;
            for (const Pin& p : node.inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keepInputs) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemoveIn.push_back(p.name);
            }
            for (const std::string& n : toRemoveIn) ActiveGraph().RemoveDynamicPin(node, false, n);

            std::vector<std::string> keepOutputs;
            if (returnNode) {
                for (const Pin& srcPin : returnNode->inputs) {
                    if (srcPin.type == PinType::Exec) continue;
                    ActiveGraph().AddDynamicPin(node, true, srcPin.name, srcPin.type);
                    keepOutputs.push_back(srcPin.name);
                }
            }
            std::vector<std::string> toRemoveOut;
            for (const Pin& p : node.outputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keepOutputs) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemoveOut.push_back(p.name);
            }
            for (const std::string& n : toRemoveOut) ActiveGraph().RemoveDynamicPin(node, true, n);
            isDirty = true;
        }
    }
    y += 28;

    DrawText("Inputs", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
    y += 18;
    for (const Pin& pin : node.inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Function Name") continue;
        DrawText((pin.name + " (" + ParamTypeShortName(pin.type) + ")").c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 16;
    }

    y += 8;
    DrawText("Outputs", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
    y += 18;
    for (const Pin& pin : node.outputs) {
        if (pin.type == PinType::Exec) continue;
        DrawText((pin.name + " (" + ParamTypeShortName(pin.type) + ")").c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 16;
    }
}
void StargraphEditorPanel::RenderFunctionDetails(Rectangle panel, float y, FunctionDef& fn, int detailIndex) {
    const UIPalette& theme = UITheme::Get();

    Node* entryNode = fn.graph.FindNodeByType("Function_Entry");
    Node* returnNode = fn.graph.FindNodeByType("Return");

    if (lastFuncDetailIndex != detailIndex) {
        snprintf(funcNameEditBuf, sizeof(funcNameEditBuf), "%s", fn.name.c_str());
        int row = 0;
        if (entryNode) {
            for (const Pin& pin : entryNode->outputs) {
                if (pin.type == PinType::Exec) continue;
                if (row < 8) snprintf(funcInputNameBuf[row], 64, "%s", pin.name.c_str());
                row++;
            }
        }
        row = 0;
        if (returnNode) {
            for (const Pin& pin : returnNode->inputs) {
                if (pin.type == PinType::Exec) continue;
                if (row < 8) snprintf(funcOutputNameBuf[row], 64, "%s", pin.name.c_str());
                row++;
            }
        }
        lastFuncDetailIndex = detailIndex;
        funcDetailActiveField = 0;
    }

    DrawText("Name", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    std::string nameVal = fn.name;
    if (PropertyField::StringBox(nameRect, funcNameEditBuf, sizeof(funcNameEditBuf), 1, funcDetailActiveField, nameVal)) {
        if (!nameVal.empty()) graph.RenameFunction(detailIndex, nameVal);
        isDirty = true;
    }
    y += 26;

    y += 6;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, theme.panelBorder);
    y += 10;
    DrawText("Inputs", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
    y += 18;

    if (entryNode) {
        std::string pinToRemove;
        int row = 0;
        for (Pin& pin : entryNode->outputs) {
            if (pin.type == PinType::Exec) continue;

            if (row < 8) {
                Rectangle nameR = {panel.x + 8, y, panel.width - 16 - 66, 18};
                std::string pname = pin.name;
                if (PropertyField::StringBox(nameR, funcInputNameBuf[row], 64, 10 + row, funcDetailActiveField, pname)) {
                    pin.name = pname;
                    isDirty = true;
                }
            }

            Rectangle typeR = {panel.x + panel.width - 54, y, 30, 18};
            if (UIWidget::Button(typeR, ParamTypeShortName(pin.type))) {
                pin.type = NextParamType(pin.type);
                isDirty = true;
            }

            Rectangle delR = {panel.x + panel.width - 20, y, 18, 18};
            if (UIWidget::Button(delR, "X")) pinToRemove = pin.name;

            y += 22;
            row++;
        }

        if (!pinToRemove.empty()) {
            fn.graph.RemoveDynamicPin(*entryNode, true, pinToRemove);
            isDirty = true;
        }

        y += 6;
        if (row < 8) {
            if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 20}, "+ Add Input")) {
                std::string newName;
                int n = 1;
                do {
                    newName = "Input" + std::to_string(n++);
                    bool taken = false;
                    for (const Pin& p : entryNode->outputs) if (p.name == newName) { taken = true; break; }
                    if (!taken) break;
                } while (n < 100);
                fn.graph.AddDynamicPin(*entryNode, true, newName, PinType::Float);
                isDirty = true;
            }
            y += 26;
        }
    }

    y += 6;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, theme.panelBorder);
    y += 10;
    DrawText("Outputs", (int)panel.x + 8, (int)y, 12, theme.textSecondary);
    y += 18;

    if (returnNode) {
        std::string pinToRemove;
        int row = 0;
        for (Pin& pin : returnNode->inputs) {
            if (pin.type == PinType::Exec) continue;

            if (row < 8) {
                Rectangle nameR = {panel.x + 8, y, panel.width - 16 - 66, 18};
                std::string pname = pin.name;
                if (PropertyField::StringBox(nameR, funcOutputNameBuf[row], 64, 20 + row, funcDetailActiveField, pname)) {
                    pin.name = pname;
                    isDirty = true;
                }
            }

            Rectangle typeR = {panel.x + panel.width - 54, y, 30, 18};
            if (UIWidget::Button(typeR, ParamTypeShortName(pin.type))) {
                pin.type = NextParamType(pin.type);
                isDirty = true;
            }

            Rectangle delR = {panel.x + panel.width - 20, y, 18, 18};
            if (UIWidget::Button(delR, "X")) pinToRemove = pin.name;

            y += 22;
            row++;
        }

        if (!pinToRemove.empty()) {
            fn.graph.RemoveDynamicPin(*returnNode, false, pinToRemove);
            isDirty = true;
        }

        y += 6;
        if (row < 8) {
            if (UIWidget::Button({panel.x + 8, y, panel.width - 16, 20}, "+ Add Output")) {
                std::string newName;
                int n = 1;
                do {
                    newName = "Output" + std::to_string(n++);
                    bool taken = false;
                    for (const Pin& p : returnNode->inputs) if (p.name == newName) { taken = true; break; }
                    if (!taken) break;
                } while (n < 100);
                fn.graph.AddDynamicPin(*returnNode, false, newName, PinType::Float);
                isDirty = true;
            }
        }
    }
}
