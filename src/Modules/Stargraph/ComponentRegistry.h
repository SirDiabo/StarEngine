#pragma once
#include "Stargraph.h"
#include <functional>
#include <string>
#include <vector>

class Component;

enum class ComponentFieldKind { String, AssetPath, Float, Bool, Enum, Device };

struct ComponentFieldDesc {
    std::string key;
    std::string label;
    ComponentFieldKind kind = ComponentFieldKind::String;
    std::string defaultValue;
    std::string assetExtension;
    std::vector<std::string> enumOptions;
    std::function<bool(const ComponentDecl&)> visibleIf;
    std::function<void(Component&, const ComponentDecl&)> apply;
};

using ComponentFactory = std::function<Component*()>;

struct ComponentTypeInfo {
    std::string componentType;
    ComponentFactory factory;
    std::vector<ComponentFieldDesc> fields;
    std::function<void(Component&, const ComponentDecl&)> applyArrays;
};

class ComponentRegistry {
public:
    static ComponentRegistry& Get();

    void Register(ComponentTypeInfo info);
    const std::vector<ComponentTypeInfo>& GetAll() const { return types; }
    const ComponentTypeInfo* Find(const std::string& componentType) const;

private:
    std::vector<ComponentTypeInfo> types;
};

struct ComponentRegistrar {
    ComponentRegistrar(ComponentTypeInfo info) { ComponentRegistry::Get().Register(std::move(info)); }
};

namespace ComponentField {
    ComponentFieldDesc Str(const std::string& key, const std::string& label, const std::string& defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply);
    ComponentFieldDesc Asset(const std::string& key, const std::string& label, const std::string& extension,
                              std::function<void(Component&, const ComponentDecl&)> apply);
    ComponentFieldDesc Flt(const std::string& key, const std::string& label, float defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply);
    ComponentFieldDesc Bln(const std::string& key, const std::string& label, bool defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply);
    ComponentFieldDesc Enm(const std::string& key, const std::string& label, const std::vector<std::string>& options,
                            const std::string& defaultValue, std::function<void(Component&, const ComponentDecl&)> apply);
    ComponentFieldDesc Dev(const std::string& key, const std::string& label,
                            std::function<void(Component&, const ComponentDecl&)> apply);
}
