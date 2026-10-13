#include "ComponentRegistry.h"
#include "Runtime/Components/Component.h"
#include "Core/SafeParse.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>

ComponentRegistry& ComponentRegistry::Get() {
    static ComponentRegistry instance;
    return instance;
}

void ComponentRegistry::Register(ComponentTypeInfo info) {
    types.push_back(std::move(info));
}

const ComponentTypeInfo* ComponentRegistry::Find(const std::string& componentType) const {
    for (const ComponentTypeInfo& info : types) {
        if (info.componentType == componentType) return &info;
    }
    return nullptr;
}

static int editorApplyDepth = 0;

bool ComponentRegistry::IsEditorApply() { return editorApplyDepth > 0; }
ComponentRegistry::EditorApplyScope::EditorApplyScope() { editorApplyDepth++; }
ComponentRegistry::EditorApplyScope::~EditorApplyScope() { editorApplyDepth--; }

bool ComponentRegistry::CaptureFields(const std::string& componentType, const Component& component, ComponentDecl& outDecl) const {
    const ComponentTypeInfo* info = Find(componentType);
    if (!info) return false;
    for (const ComponentFieldDesc& field : info->fields) {
        if (field.read) outDecl.fields[field.key] = field.read(component);
    }
    if (info->readArrays) info->readArrays(component, outDecl);
    return true;
}

namespace {

bool ValuesMatch(const ComponentFieldDesc& field, const std::string& expected, const std::string& actual) {
    if (field.kind == ComponentFieldKind::Float) {
        return std::fabs(SafeParse::ToFloat(expected) - SafeParse::ToFloat(actual)) < 1e-4f;
    }
    if (field.kind == ComponentFieldKind::Bool) {
        return (expected == "true") == (actual == "true");
    }
    return expected == actual;
}

std::vector<std::string> ProbeValues(const ComponentFieldDesc& field) {
    switch (field.kind) {
        case ComponentFieldKind::Float: return { "7.25", "-3.5" };
        case ComponentFieldKind::Bool: return { "true", "false" };
        case ComponentFieldKind::Enum: return field.enumOptions;
        case ComponentFieldKind::AssetPath: return { "Test/Probe" + field.assetExtension };
        case ComponentFieldKind::Device: return { "Probe" };
        default: return { "probe" };
    }
}

}

int ComponentRegistry::VerifyRoundTrip() const {
    EditorApplyScope editorScope;
    int checked = 0;
    int failures = 0;
    for (const ComponentTypeInfo& info : types) {
        if (!info.factory) continue;
        for (const ComponentFieldDesc& field : info.fields) {
            if (!field.apply) {
                TraceLog(LOG_WARNING, "ComponentRoundTrip: %s.%s has no apply", info.componentType.c_str(), field.key.c_str());
                failures++;
                continue;
            }
            if (!field.read) {
                TraceLog(LOG_WARNING, "ComponentRoundTrip: %s.%s has no read", info.componentType.c_str(), field.key.c_str());
                failures++;
                continue;
            }
            for (const std::string& probe : ProbeValues(field)) {
                Component* component = info.factory();
                ComponentDecl decl;
                decl.componentType = info.componentType;
                decl.fields[field.key] = probe;
                field.apply(*component, decl);
                std::string actual = field.read(*component);
                checked++;
                if (!ValuesMatch(field, probe, actual)) {
                    TraceLog(LOG_WARNING, "ComponentRoundTrip: %s.%s wrote \"%s\" but read back \"%s\"",
                             info.componentType.c_str(), field.key.c_str(), probe.c_str(), actual.c_str());
                    failures++;
                }
                delete component;
            }
        }
        if (info.applyArrays || info.readArrays) {
            for (const std::string& key : info.arrayKeys) {
                Component* component = info.factory();
                ComponentDecl decl;
                decl.arrayFields[key] = { "a", "b" };
                if (info.applyArrays) info.applyArrays(*component, decl);
                ComponentDecl readBack;
                if (info.readArrays) info.readArrays(*component, readBack);
                checked++;
                auto it = readBack.arrayFields.find(key);
                if (it == readBack.arrayFields.end() || it->second != decl.arrayFields[key]) {
                    TraceLog(LOG_WARNING, "ComponentRoundTrip: %s array %s did not round-trip", info.componentType.c_str(), key.c_str());
                    failures++;
                }
                delete component;
            }
        }
    }
    TraceLog(failures == 0 ? LOG_INFO : LOG_WARNING, "ComponentRoundTrip: %d probes, %d failures", checked, failures);
    return failures;
}

namespace ComponentField {

std::string FloatStr(float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", value);
    return buf;
}

std::string BoolStr(bool value) {
    return value ? "true" : "false";
}

ComponentFieldDesc Str(const std::string& key, const std::string& label, const std::string& defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::String;
    d.defaultValue = defaultValue;
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

ComponentFieldDesc Asset(const std::string& key, const std::string& label, const std::string& extension,
                          std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::AssetPath;
    d.assetExtension = extension;
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

ComponentFieldDesc Flt(const std::string& key, const std::string& label, float defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Float;
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", defaultValue);
    d.defaultValue = buf;
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

ComponentFieldDesc Bln(const std::string& key, const std::string& label, bool defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Bool;
    d.defaultValue = defaultValue ? "true" : "false";
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

ComponentFieldDesc Enm(const std::string& key, const std::string& label, const std::vector<std::string>& options,
                        const std::string& defaultValue, std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Enum;
    d.enumOptions = options;
    d.defaultValue = defaultValue;
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

ComponentFieldDesc Dev(const std::string& key, const std::string& label,
                        std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Device;
    d.defaultValue = "Unclaimed";
    d.apply = std::move(apply);
    d.read = std::move(read);
    return d;
}

}
