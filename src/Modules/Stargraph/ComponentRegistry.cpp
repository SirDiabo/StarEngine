#include "ComponentRegistry.h"

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

namespace ComponentField {

ComponentFieldDesc Str(const std::string& key, const std::string& label, const std::string& defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::String;
    d.defaultValue = defaultValue;
    d.apply = std::move(apply);
    return d;
}

ComponentFieldDesc Asset(const std::string& key, const std::string& label, const std::string& extension,
                          std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::AssetPath;
    d.assetExtension = extension;
    d.apply = std::move(apply);
    return d;
}

ComponentFieldDesc Flt(const std::string& key, const std::string& label, float defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Float;
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", defaultValue);
    d.defaultValue = buf;
    d.apply = std::move(apply);
    return d;
}

ComponentFieldDesc Bln(const std::string& key, const std::string& label, bool defaultValue,
                        std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Bool;
    d.defaultValue = defaultValue ? "true" : "false";
    d.apply = std::move(apply);
    return d;
}

ComponentFieldDesc Enm(const std::string& key, const std::string& label, const std::vector<std::string>& options,
                        const std::string& defaultValue, std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Enum;
    d.enumOptions = options;
    d.defaultValue = defaultValue;
    d.apply = std::move(apply);
    return d;
}

ComponentFieldDesc Dev(const std::string& key, const std::string& label,
                        std::function<void(Component&, const ComponentDecl&)> apply) {
    ComponentFieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = ComponentFieldKind::Device;
    d.defaultValue = "Unclaimed";
    d.apply = std::move(apply);
    return d;
}

}
