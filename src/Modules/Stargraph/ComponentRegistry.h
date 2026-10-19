#pragma once
#include "Stargraph.h"
#include <functional>
#include <string>
#include <vector>

class Component;

using ComponentReader = std::function<std::string(const Component&)>;

enum class ComponentFieldKind { String, AssetPath, Float, Bool, Enum, Device, LayerMask };

struct ComponentFieldDesc {
    std::string key;
    std::string label;
    ComponentFieldKind kind = ComponentFieldKind::String;
    std::string defaultValue;
    std::string assetExtension;
    std::vector<std::string> enumOptions;
    std::function<std::vector<std::string>()> enumProvider;
    std::function<bool(const ComponentDecl&)> visibleIf;
    std::vector<std::string> colorKeys;
    bool hiddenInDetails = false;
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
    inline std::vector<std::string> EnumOptionsOf(const ComponentFieldDesc& field) {
        return field.enumProvider ? field.enumProvider() : field.enumOptions;
    }
    inline int ChannelToByte(float value) {
        float scaled = value * 255.0f + 0.5f;
        return scaled < 0.0f ? 0 : scaled > 255.0f ? 255 : (int)scaled;
    }
    inline float ByteToChannel(int value) { return (float)(value < 0 ? 0 : value > 255 ? 255 : value) / 255.0f; }
    inline ComponentFieldDesc AsColorRGB(ComponentFieldDesc firstChannel, const std::string& label, std::vector<std::string> channelKeys) {
        firstChannel.label = label;
        firstChannel.colorKeys = std::move(channelKeys);
        return firstChannel;
    }
    inline ComponentFieldDesc HiddenInDetails(ComponentFieldDesc field) {
        field.hiddenInDetails = true;
        return field;
    }
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
