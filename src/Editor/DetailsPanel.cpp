#include "DetailsPanel.h"
#include "Core/StarAsset.h"
#include "PropertyFields.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/SafeParse.h"
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

    snprintf(compLightColorBuf[0], 32, "%.2f", SafeParse::ToFloat(inst.component.Get("LightColorR", "1")));
    snprintf(compLightColorBuf[1], 32, "%.2f", SafeParse::ToFloat(inst.component.Get("LightColorG", "1")));
    snprintf(compLightColorBuf[2], 32, "%.2f", SafeParse::ToFloat(inst.component.Get("LightColorB", "1")));
    snprintf(compLightIntensityBuf, sizeof(compLightIntensityBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightIntensity", "1")));
    snprintf(compLightRadiusBuf, sizeof(compLightRadiusBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightRadius", "500")));
    snprintf(compLightInnerConeBuf, sizeof(compLightInnerConeBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightInnerConeAngle", "25")));
    snprintf(compLightOuterConeBuf, sizeof(compLightOuterConeBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightOuterConeAngle", "35")));
    snprintf(compLightWidthBuf, sizeof(compLightWidthBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightWidth", "100")));
    snprintf(compLightHeightBuf, sizeof(compLightHeightBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("LightHeight", "100")));
    snprintf(compCameraFovBuf, sizeof(compCameraFovBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("CameraFOV", "50")));
    snprintf(compCameraNearBuf, sizeof(compCameraNearBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("CameraNearClip", "0.1")));
    snprintf(compCameraFarBuf, sizeof(compCameraFarBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("CameraFarClip", "4000")));
    snprintf(compAudioVolumeBuf, sizeof(compAudioVolumeBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("Volume", "1")));
    snprintf(compAudioPitchBuf, sizeof(compAudioPitchBuf), "%.2f", SafeParse::ToFloat(inst.component.Get("Pitch", "1")));

    activeField = 0;
    meshPicker.Close();
    materialPicker.Close();
    materialSlotPickerIndex = -1;
    audioPathPicker.Close();
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

    auto refreshField = [&](char* buf, size_t bufSize, int fieldId, float val) {
        if (activeField == fieldId) return;
        snprintf(buf, bufSize, "%.2f", val);
    };
    refreshField(posXBuf, sizeof(posXBuf), 2, inst.posX);
    refreshField(posYBuf, sizeof(posYBuf), 3, inst.posY);
    refreshField(posZBuf, sizeof(posZBuf), 4, inst.posZ);
    refreshField(rotXBuf, sizeof(rotXBuf), 5, inst.rotX);
    refreshField(rotYBuf, sizeof(rotYBuf), 6, inst.rotY);
    refreshField(rotZBuf, sizeof(rotZBuf), 7, inst.rotZ);
    refreshField(scaleXBuf, sizeof(scaleXBuf), 8, inst.scaleX);
    refreshField(scaleYBuf, sizeof(scaleYBuf), 9, inst.scaleY);
    refreshField(scaleZBuf, sizeof(scaleZBuf), 10, inst.scaleZ);

    auto vec3Row = [&](char bufX[32], char bufY[32], char bufZ[32], int fieldBase,
                        float& vx, float& vy, float& vz) {
        Rectangle row = {x + 8, y, w - 16, 18};
        if (PropertyField::Vec3Row(row, bufX, bufY, bufZ, fieldBase, activeField, vx, vy, vz)) {
            if (onEdited) onEdited();
        }
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
        if (onEdited) onEdited();
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
    } else if (inst.hasComponent) {
        RenderInlineComponentSection(inst, x, y, w);
    } else if (inst.className == "StaticMeshActor") {
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
    if (meshPicker.Render(pickedMesh)) {
        inst.meshAssetPath = pickedMesh;
        if (onEdited) onEdited();
    }

    std::vector<MaterialSlot> meshSlots;
    if (!inst.meshAssetPath.empty()) {
        StarAsset meshAsset;
        if (StarAsset::LoadFromFile(inst.meshAssetPath, meshAsset)) meshSlots = meshAsset.materialSlots;
    }
    if (inst.materialAssetPaths.size() != meshSlots.size()) inst.materialAssetPaths.resize(meshSlots.size());

    bool castsShadow = inst.castsShadow;
    if (GuiCheckBox({x + 8, y, 18, 18}, "Casts Shadow", &castsShadow) && !anyPickerOpen) {
        inst.castsShadow = castsShadow;
        if (onEdited) onEdited();
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
        if (onEdited) onEdited();
    }
}

void DetailsPanel::RenderInlineComponentSection(ActorInstance& inst, float x, float& y, float w) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    ComponentDecl& comp = inst.component;

    DrawText((comp.componentType + " Component").c_str(), (int)x + 8, (int)y, 12, theme.sectionHeader);
    y += 18;

    auto floatFieldGeneric = [&](const char* label, char* buf, size_t bufSize, int fieldId,
                                  ComponentDecl& c, const char* key, const char* defaultValue) {
        float outVal = SafeParse::ToFloat(c.Get(key, defaultValue));
        DrawText(label, (int)x + 8, (int)y, 11, theme.textSecondary);
        y += 14;
        Rectangle r = {x + 8, y, w - 16, 18};
        if (PropertyField::FloatBox(r, buf, bufSize, fieldId, activeField, outVal)) {
            char vbuf[32];
            snprintf(vbuf, sizeof(vbuf), "%g", outVal);
            c.fields[key] = vbuf;
            if (onEdited) onEdited();
        }
        y += 22;
    };

    if (comp.componentType == "LightComponent") {
        DrawText("Light Type", (int)x + 8, (int)y, 11, theme.textSecondary);
        y += 14;
        std::string lightType = comp.Get("LightType", "Point");
        if (UIWidget::Button({x + 8, y, w - 16, 18}, lightType.c_str())) {
            if (lightType == "Point") lightType = "Rectangle";
            else if (lightType == "Rectangle") lightType = "Spot";
            else if (lightType == "Spot") lightType = "Global";
            else if (lightType == "Global") lightType = "Sun";
            else lightType = "Point";
            comp.fields["LightType"] = lightType;
            if (onEdited) onEdited();
        }
        y += 24;

        DrawText("Light Color", (int)x + 8, (int)y, 11, theme.sectionHeader);
        y += 14;
        Rectangle colorRow = {x + 8, y, w - 16, 18};
        float lightColorR = SafeParse::ToFloat(comp.Get("LightColorR", "1"));
        float lightColorG = SafeParse::ToFloat(comp.Get("LightColorG", "1"));
        float lightColorB = SafeParse::ToFloat(comp.Get("LightColorB", "1"));
        if (PropertyField::Vec3Row(colorRow, compLightColorBuf[0], compLightColorBuf[1], compLightColorBuf[2],
                                    16, activeField, lightColorR, lightColorG, lightColorB)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", lightColorR); comp.fields["LightColorR"] = buf;
            snprintf(buf, sizeof(buf), "%g", lightColorG); comp.fields["LightColorG"] = buf;
            snprintf(buf, sizeof(buf), "%g", lightColorB); comp.fields["LightColorB"] = buf;
            if (onEdited) onEdited();
        }
        y += 22;

        floatFieldGeneric("Intensity", compLightIntensityBuf, sizeof(compLightIntensityBuf), 19, comp, "LightIntensity", "1");

        if (lightType != "Global" && lightType != "Sun") {
            floatFieldGeneric("Radius", compLightRadiusBuf, sizeof(compLightRadiusBuf), 20, comp, "LightRadius", "500");
        }
        if (lightType == "Spot") {
            floatFieldGeneric("Inner Cone Angle", compLightInnerConeBuf, sizeof(compLightInnerConeBuf), 21, comp, "LightInnerConeAngle", "25");
            floatFieldGeneric("Outer Cone Angle", compLightOuterConeBuf, sizeof(compLightOuterConeBuf), 22, comp, "LightOuterConeAngle", "35");
        }
        if (lightType == "Rectangle") {
            floatFieldGeneric("Width", compLightWidthBuf, sizeof(compLightWidthBuf), 23, comp, "LightWidth", "100");
            floatFieldGeneric("Height", compLightHeightBuf, sizeof(compLightHeightBuf), 24, comp, "LightHeight", "100");
        }
    } else if (comp.componentType == "CameraComponent") {
        floatFieldGeneric("FOV", compCameraFovBuf, sizeof(compCameraFovBuf), 25, comp, "CameraFOV", "50");
        floatFieldGeneric("Near Clip", compCameraNearBuf, sizeof(compCameraNearBuf), 26, comp, "CameraNearClip", "0.1");
        floatFieldGeneric("Far Clip", compCameraFarBuf, sizeof(compCameraFarBuf), 27, comp, "CameraFarClip", "4000");
        bool ortho = comp.Get("CameraOrthographic", "false") == "true";
        if (GuiCheckBox({x + 8, y, 18, 18}, "Orthographic", &ortho)) {
            comp.fields["CameraOrthographic"] = ortho ? "true" : "false";
            if (onEdited) onEdited();
        }
        y += 24;
    } else if (comp.componentType == "AudioComponent") {
        DrawText("Sound", (int)x + 8, (int)y, 11, theme.textSecondary);
        y += 14;
        Rectangle r = {x + 8, y, w - 16, 18};
        DrawRectangleRec(r, theme.fieldBg);
        DrawRectangleLinesEx(r, 1, theme.fieldBorder);
        std::string soundAssetPath = comp.Get("Sound");
        DrawText(AssetDisplayName(soundAssetPath).c_str(), (int)r.x + 4, (int)r.y + 3, 11, theme.textPrimary);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !audioPathPicker.IsOpen()) {
            audioPathPicker.Open({r.x, r.y + r.height}, ".starsound");
        }
        y += 22;
        std::string pickedSound;
        if (audioPathPicker.Render(pickedSound)) {
            comp.fields["Sound"] = pickedSound;
            if (onEdited) onEdited();
        }

        floatFieldGeneric("Volume", compAudioVolumeBuf, sizeof(compAudioVolumeBuf), 28, comp, "Volume", "1");
        floatFieldGeneric("Pitch", compAudioPitchBuf, sizeof(compAudioPitchBuf), 29, comp, "Pitch", "1");

        bool autoPlay = comp.Get("AutoPlay", "false") == "true";
        if (GuiCheckBox({x + 8, y, 18, 18}, "Auto Play", &autoPlay)) {
            comp.fields["AutoPlay"] = autoPlay ? "true" : "false";
            if (onEdited) onEdited();
        }
        y += 24;
        bool looping = comp.Get("Looping", "false") == "true";
        if (GuiCheckBox({x + 8, y, 18, 18}, "Looping", &looping)) {
            comp.fields["Looping"] = looping ? "true" : "false";
            if (onEdited) onEdited();
        }
        y += 24;
    }
}

void DetailsPanel::RenderClassComponentsSection(const ActorInstance& inst, float x, float& y, float w) {
    const UIPalette& theme = UITheme::Get();

    const Stargraph* classGraph = StargraphAssetCache::Get().GetOrLoad(inst.graphAssetPath);

    DrawText("Class", (int)x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    DrawText(AssetDisplayName(inst.graphAssetPath).c_str(), (int)x + 8, (int)y, 11, theme.textPrimary);
    y += 18;

    Rectangle editBtn = {x + 8, y, w - 16, 20};
    if (UIWidget::Button(editBtn, "Edit Class")) {
        if (onOpenAsset) onOpenAsset(inst.graphAssetPath, fs::path(inst.graphAssetPath).extension().string());
    }
    y += 28;

    if (!classGraph) {
        DrawText("Could not load class asset.", (int)x + 8, (int)y, 11, {110, 110, 118, 255});
        y += 16;
        return;
    }

    const auto& comps = classGraph->GetComponents();
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