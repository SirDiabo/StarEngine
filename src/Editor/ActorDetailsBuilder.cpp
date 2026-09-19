#include "ActorDetailsBuilder.h"
#include "Core/StarAsset.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/SafeParse.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

static std::string AssetDisplayName(const std::string& assetPath) {
    if (assetPath.empty()) return "None";
    return fs::path(assetPath).stem().string();
}

static void BuildStaticMeshRows(ActorInstance& inst, std::vector<PropertyRow>& rows,
                                 const std::function<ActorInstance*()>& currentInstance,
                                 const std::function<void()>& onEdited) {
    PropertyRow header;
    header.kind = PropertyRowKind::Header;
    header.label = "Mesh Component";
    rows.push_back(header);

    PropertyRow meshRow;
    meshRow.id = "mesh";
    meshRow.kind = PropertyRowKind::Asset;
    meshRow.label = "Static Mesh";
    meshRow.stringValue = AssetDisplayName(inst.meshAssetPath);
    meshRow.assetExtension = ".starasset";
    rows.push_back(meshRow);

    PropertyRow shadowRow;
    shadowRow.id = "castsShadow";
    shadowRow.kind = PropertyRowKind::Bool;
    shadowRow.label = "Casts Shadow";
    shadowRow.boolValue = inst.castsShadow;
    shadowRow.setBool = [currentInstance, onEdited](bool v) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        i->castsShadow = v;
        if (onEdited) onEdited();
    };
    rows.push_back(shadowRow);

    std::vector<MaterialSlot> meshSlots;
    if (!inst.meshAssetPath.empty()) {
        StarAsset meshAsset;
        if (StarAsset::LoadFromFile(inst.meshAssetPath, meshAsset)) meshSlots = meshAsset.materialSlots;
    }
    if (inst.materialAssetPaths.size() != meshSlots.size()) inst.materialAssetPaths.resize(meshSlots.size());

    if (meshSlots.empty()) {
        PropertyRow empty;
        empty.kind = PropertyRowKind::ReadOnlyText;
        empty.label = "Materials";
        empty.stringValue = "No material slots";
        rows.push_back(empty);
        return;
    }

    PropertyRow matHeader;
    matHeader.kind = PropertyRowKind::Header;
    matHeader.label = "Materials";
    rows.push_back(matHeader);

    for (size_t i = 0; i < meshSlots.size(); i++) {
        PropertyRow matRow;
        matRow.id = "material:" + std::to_string(i);
        matRow.kind = PropertyRowKind::Asset;
        matRow.label = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
        matRow.stringValue = AssetDisplayName(inst.materialAssetPaths[i]);
        matRow.assetExtension = ".starmat";
        rows.push_back(matRow);
    }
}

static void BuildInlineComponentRows(ActorInstance& inst, std::vector<PropertyRow>& rows,
                                      const std::function<ActorInstance*()>& currentInstance,
                                      const std::function<void()>& onEdited) {
    ComponentDecl& comp = inst.component;

    PropertyRow header;
    header.kind = PropertyRowKind::Header;
    header.label = comp.componentType + " Component";
    rows.push_back(header);

    auto floatField = [&](const char* label, const char* key, const char* defaultValue) {
        PropertyRow row;
        row.id = std::string("field:") + key;
        row.kind = PropertyRowKind::Float;
        row.label = label;
        row.floatValue = SafeParse::ToFloat(comp.Get(key, defaultValue));
        std::string keyStr = key;
        row.setFloat = [currentInstance, onEdited, keyStr](float v) {
            ActorInstance* i = currentInstance();
            if (!i) return;
            char vbuf[32];
            snprintf(vbuf, sizeof(vbuf), "%g", v);
            i->component.fields[keyStr] = vbuf;
            if (onEdited) onEdited();
        };
        rows.push_back(row);
    };

    auto boolField = [&](const char* label, const char* key, const char* defaultValue) {
        PropertyRow row;
        row.id = std::string("field:") + key;
        row.kind = PropertyRowKind::Bool;
        row.label = label;
        row.boolValue = comp.Get(key, defaultValue) == "true";
        std::string keyStr = key;
        row.setBool = [currentInstance, onEdited, keyStr](bool v) {
            ActorInstance* i = currentInstance();
            if (!i) return;
            i->component.fields[keyStr] = v ? "true" : "false";
            if (onEdited) onEdited();
        };
        rows.push_back(row);
    };

    if (comp.componentType == "LightComponent") {
        std::string lightType = comp.Get("LightType", "Point");

        PropertyRow typeRow;
        typeRow.id = "field:LightType";
        typeRow.kind = PropertyRowKind::Enum;
        typeRow.label = "Light Type";
        typeRow.stringValue = lightType;
        typeRow.enumOptions = { "Point", "Rectangle", "Spot", "Global", "Sun" };
        typeRow.setString = [currentInstance, onEdited](const std::string& v) {
            ActorInstance* i = currentInstance();
            if (!i) return;
            i->component.fields["LightType"] = v;
            if (onEdited) onEdited();
        };
        rows.push_back(typeRow);

        PropertyRow colorRow;
        colorRow.id = "field:LightColor";
        colorRow.kind = PropertyRowKind::Vec3;
        colorRow.label = "Light Color";
        colorRow.vec3[0] = SafeParse::ToFloat(comp.Get("LightColorR", "1"));
        colorRow.vec3[1] = SafeParse::ToFloat(comp.Get("LightColorG", "1"));
        colorRow.vec3[2] = SafeParse::ToFloat(comp.Get("LightColorB", "1"));
        colorRow.setVec3 = [currentInstance, onEdited](float r, float g, float b) {
            ActorInstance* i = currentInstance();
            if (!i) return;
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", r); i->component.fields["LightColorR"] = buf;
            snprintf(buf, sizeof(buf), "%g", g); i->component.fields["LightColorG"] = buf;
            snprintf(buf, sizeof(buf), "%g", b); i->component.fields["LightColorB"] = buf;
            if (onEdited) onEdited();
        };
        rows.push_back(colorRow);

        floatField("Intensity", "LightIntensity", "1");

        if (lightType != "Global" && lightType != "Sun") {
            floatField("Radius", "LightRadius", "500");
        }
        if (lightType == "Spot") {
            floatField("Inner Cone Angle", "LightInnerConeAngle", "25");
            floatField("Outer Cone Angle", "LightOuterConeAngle", "35");
        }
        if (lightType == "Rectangle") {
            floatField("Width", "LightWidth", "100");
            floatField("Height", "LightHeight", "100");
        }
    } else if (comp.componentType == "CameraComponent") {
        floatField("FOV", "CameraFOV", "50");
        floatField("Near Clip", "CameraNearClip", "0.1");
        floatField("Far Clip", "CameraFarClip", "4000");
        boolField("Orthographic", "CameraOrthographic", "false");
    } else if (comp.componentType == "AudioComponent") {
        PropertyRow soundRow;
        soundRow.id = "sound";
        soundRow.kind = PropertyRowKind::Asset;
        soundRow.label = "Sound";
        soundRow.stringValue = AssetDisplayName(comp.Get("Sound"));
        soundRow.assetExtension = ".starsound";
        rows.push_back(soundRow);

        floatField("Volume", "Volume", "1");
        floatField("Pitch", "Pitch", "1");
        boolField("Auto Play", "AutoPlay", "false");
        boolField("Looping", "Looping", "false");
    }
}

static void BuildClassComponentsRows(const ActorInstance& inst, std::vector<PropertyRow>& rows,
                                      const std::function<void(const std::string&, const std::string&)>& onOpenAsset) {
    const Stargraph* classGraph = StargraphAssetCache::Get().GetOrLoad(inst.graphAssetPath);

    PropertyRow classRow;
    classRow.kind = PropertyRowKind::ReadOnlyText;
    classRow.label = "Class";
    classRow.stringValue = AssetDisplayName(inst.graphAssetPath);
    rows.push_back(classRow);

    PropertyRow editBtn;
    editBtn.kind = PropertyRowKind::Button;
    editBtn.label = "Edit Class";
    std::string graphPath = inst.graphAssetPath;
    editBtn.onButtonPressed = [onOpenAsset, graphPath]() {
        if (onOpenAsset) onOpenAsset(graphPath, fs::path(graphPath).extension().string());
    };
    rows.push_back(editBtn);

    if (!classGraph) {
        PropertyRow msg;
        msg.kind = PropertyRowKind::ReadOnlyText;
        msg.label = "";
        msg.stringValue = "Could not load class asset.";
        rows.push_back(msg);
        return;
    }

    const auto& comps = classGraph->GetComponents();
    if (comps.empty()) {
        PropertyRow msg;
        msg.kind = PropertyRowKind::ReadOnlyText;
        msg.label = "";
        msg.stringValue = "No components on this class.";
        rows.push_back(msg);
        return;
    }

    PropertyRow header;
    header.kind = PropertyRowKind::Header;
    header.label = "Components";
    rows.push_back(header);

    for (const ComponentDecl& comp : comps) {
        PropertyRow row;
        row.kind = PropertyRowKind::ReadOnlyText;
        row.label = comp.name;
        row.stringValue = "(" + comp.componentType + ")";
        rows.push_back(row);
    }

    PropertyRow hint;
    hint.kind = PropertyRowKind::ReadOnlyText;
    hint.label = "";
    hint.stringValue = "Edit components in the class blueprint.";
    rows.push_back(hint);
}

std::vector<PropertyRow> BuildActorPropertyRows(
    ActorInstance& inst,
    const std::function<ActorInstance*()>& currentInstance,
    const std::function<void()>& onEdited,
    const std::function<void(const std::string&, const std::string&)>& onOpenAsset) {

    std::vector<PropertyRow> rows;

    PropertyRow classHeader;
    classHeader.kind = PropertyRowKind::Header;
    classHeader.label = inst.className;
    rows.push_back(classHeader);

    PropertyRow nameRow;
    nameRow.id = "name";
    nameRow.kind = PropertyRowKind::String;
    nameRow.label = "Name";
    nameRow.stringValue = inst.name;
    nameRow.setString = [currentInstance, onEdited](const std::string& v) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        i->name = v;
        if (onEdited) onEdited();
    };
    rows.push_back(nameRow);

    auto vec3Row = [&](const char* id, const char* label, float x, float y, float z,
                        std::function<void(float, float, float)> setter) {
        PropertyRow row;
        row.id = id;
        row.kind = PropertyRowKind::Vec3;
        row.label = label;
        row.vec3[0] = x; row.vec3[1] = y; row.vec3[2] = z;
        row.setVec3 = setter;
        rows.push_back(row);
    };

    vec3Row("position", "Position", inst.posX, inst.posY, inst.posZ, [currentInstance, onEdited](float x, float y, float z) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        i->posX = x; i->posY = y; i->posZ = z;
        if (onEdited) onEdited();
    });
    vec3Row("rotation", "Rotation", inst.rotX, inst.rotY, inst.rotZ, [currentInstance, onEdited](float x, float y, float z) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        i->rotX = x; i->rotY = y; i->rotZ = z;
        if (onEdited) onEdited();
    });
    vec3Row("scale", "Scale", inst.scaleX, inst.scaleY, inst.scaleZ, [currentInstance, onEdited](float x, float y, float z) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        i->scaleX = x; i->scaleY = y; i->scaleZ = z;
        if (onEdited) onEdited();
    });

    if (!inst.graphAssetPath.empty()) {
        BuildClassComponentsRows(inst, rows, onOpenAsset);
    } else if (inst.hasComponent) {
        BuildInlineComponentRows(inst, rows, currentInstance, onEdited);
    } else if (inst.className == "StaticMeshActor") {
        BuildStaticMeshRows(inst, rows, currentInstance, onEdited);
    }

    return rows;
}
