#include "ActorDetailsBuilder.h"
#include "Runtime/AssetCache.h"
#include "Modules/Stargraph/ComponentEditing.h"
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
        if (const MeshSourceInfo* source = AssetCache::Get().GetMeshSource(inst.meshAssetPath)) meshSlots = source->materialSlots;
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

static LiveComponentCache& LevelComponentCache() {
    static LiveComponentCache cache;
    return cache;
}

static void BuildInlineComponentRows(ActorInstance& inst, std::vector<PropertyRow>& rows,
                                      const std::function<ActorInstance*()>& currentInstance,
                                      const std::function<void()>& onEdited) {
    ComponentDecl& comp = inst.component;

    PropertyRow header;
    header.kind = PropertyRowKind::Header;
    header.label = comp.componentType + " Component";
    rows.push_back(header);

    const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(comp.componentType);
    if (!typeInfo) return;

    std::string cacheKey = "level:" + inst.name;
    auto writeField = [currentInstance, cacheKey](const std::string& fieldKey, const std::string& value) {
        ActorInstance* i = currentInstance();
        if (!i) return;
        ComponentEditing::WriteField(LevelComponentCache(), cacheKey, i->component, fieldKey, value);
    };

    bool colorRowAdded = false;
    for (const ComponentFieldDesc& f : typeInfo->fields) {
        if (f.visibleIf && !f.visibleIf(comp)) continue;
        std::string fieldKey = f.key;
        std::string value = ComponentEditing::ReadField(LevelComponentCache(), cacheKey, comp, f);

        if (comp.componentType == "LightComponent" && fieldKey.rfind("LightColor", 0) == 0) {
            if (colorRowAdded) continue;
            colorRowAdded = true;
            PropertyRow colorRow;
            colorRow.id = "field:LightColor";
            colorRow.kind = PropertyRowKind::Vec3;
            colorRow.label = "Light Color";
            for (int c = 0; c < 3; c++) {
                static const char* channelKeys[3] = { "LightColorR", "LightColorG", "LightColorB" };
                for (const ComponentFieldDesc& channel : typeInfo->fields) {
                    if (channel.key == channelKeys[c]) {
                        colorRow.vec3[c] = SafeParse::ToFloat(ComponentEditing::ReadField(LevelComponentCache(), cacheKey, comp, channel));
                    }
                }
            }
            colorRow.setVec3 = [writeField, onEdited](float r, float g, float b) {
                writeField("LightColorR", ComponentField::FloatStr(r));
                writeField("LightColorG", ComponentField::FloatStr(g));
                writeField("LightColorB", ComponentField::FloatStr(b));
                if (onEdited) onEdited();
            };
            rows.push_back(colorRow);
            continue;
        }

        PropertyRow row;
        row.id = "field:" + fieldKey;
        row.label = f.label;
        switch (f.kind) {
            case ComponentFieldKind::Float:
                row.kind = PropertyRowKind::Float;
                row.floatValue = SafeParse::ToFloat(value);
                row.setFloat = [writeField, onEdited, fieldKey](float v) {
                    writeField(fieldKey, ComponentField::FloatStr(v));
                    if (onEdited) onEdited();
                };
                break;
            case ComponentFieldKind::Bool:
                row.kind = PropertyRowKind::Bool;
                row.boolValue = value == "true";
                row.setBool = [writeField, onEdited, fieldKey](bool v) {
                    writeField(fieldKey, ComponentField::BoolStr(v));
                    if (onEdited) onEdited();
                };
                break;
            case ComponentFieldKind::Enum:
                row.kind = PropertyRowKind::Enum;
                row.stringValue = value;
                row.enumOptions = f.enumOptions;
                row.setString = [writeField, onEdited, fieldKey](const std::string& v) {
                    writeField(fieldKey, v);
                    if (onEdited) onEdited();
                };
                break;
            case ComponentFieldKind::String:
                row.kind = PropertyRowKind::String;
                row.stringValue = value;
                row.setString = [writeField, onEdited, fieldKey](const std::string& v) {
                    writeField(fieldKey, v);
                    if (onEdited) onEdited();
                };
                break;
            case ComponentFieldKind::AssetPath:
                if (fieldKey != "Sound") continue;
                row.id = "sound";
                row.kind = PropertyRowKind::Asset;
                row.stringValue = AssetDisplayName(value);
                row.assetExtension = f.assetExtension;
                break;
            case ComponentFieldKind::Device:
                continue;
        }
        rows.push_back(row);
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
