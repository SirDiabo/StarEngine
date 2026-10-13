#pragma once
#include "Stargraph.h"
#include <functional>
#include <string>
#include <vector>

class Component;

using ComponentReader = std::function<std::string(const Component&)>;

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
    ComponentReader read;
};

using ComponentFactory = std::function<Component*()>;

struct ComponentTypeInfo {
    std::string componentType;
    ComponentFactory factory;
    std::vector<ComponentFieldDesc> fields;
    std::function<void(Component&, const ComponentDecl&)> applyArrays;
    std::function<void(const Component&, ComponentDecl&)> readArrays;
    std::vector<std::string> arrayKeys;
};

class ComponentRegistry {
public:
    static ComponentRegistry& Get();

    void Register(ComponentTypeInfo info);
    const std::vector<ComponentTypeInfo>& GetAll() const { return types; }
    const ComponentTypeInfo* Find(const std::string& componentType) const;

    bool CaptureFields(const std::string& componentType, const Component& component, ComponentDecl& outDecl) const;
    int VerifyRoundTrip() const;

    static bool IsEditorApply();
    struct EditorApplyScope {
        EditorApplyScope();
        ~EditorApplyScope();
    };

private:
    std::vector<ComponentTypeInfo> types;
};

struct ComponentRegistrar {
    ComponentRegistrar(ComponentTypeInfo info) { ComponentRegistry::Get().Register(std::move(info)); }
};

namespace ComponentField {
    std::string FloatStr(float value);
    std::string BoolStr(bool value);
    ComponentFieldDesc Str(const std::string& key, const std::string& label, const std::string& defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
    ComponentFieldDesc Asset(const std::string& key, const std::string& label, const std::string& extension,
                              std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
    ComponentFieldDesc Flt(const std::string& key, const std::string& label, float defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
    ComponentFieldDesc Bln(const std::string& key, const std::string& label, bool defaultValue,
                            std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
    ComponentFieldDesc Enm(const std::string& key, const std::string& label, const std::vector<std::string>& options,
                            const std::string& defaultValue, std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
    ComponentFieldDesc Dev(const std::string& key, const std::string& label,
                            std::function<void(Component&, const ComponentDecl&)> apply, ComponentReader read = {});
}
