#include "ComponentFields.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/Components/AudioComponent.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/LightComponent.h"
#include "Runtime/Components/InputComponent.h"

namespace {

ComponentFieldDesc Str(const char* type, const char* key, std::string ComponentDecl::* field,
                        std::function<void(Component&, const ComponentDecl&)> apply = nullptr) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::String;
    d.stringField = field;
    d.applyToComponent = std::move(apply);
    return d;
}

ComponentFieldDesc Flt(const char* type, const char* key, float ComponentDecl::* field,
                        std::function<void(Component&, const ComponentDecl&)> apply = nullptr) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::Float;
    d.floatField = field;
    d.applyToComponent = std::move(apply);
    return d;
}

ComponentFieldDesc Bln(const char* type, const char* key, bool ComponentDecl::* field,
                        std::function<void(Component&, const ComponentDecl&)> apply = nullptr) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::Bool;
    d.boolField = field;
    d.applyToComponent = std::move(apply);
    return d;
}

ComponentFieldDesc StrArr(const char* type, const char* key, std::vector<std::string> ComponentDecl::* field,
                           std::function<void(Component&, const ComponentDecl&)> apply = nullptr) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::StringArray;
    d.stringArrayField = field;
    d.applyToComponent = std::move(apply);
    return d;
}

} // namespace

const std::vector<ComponentFieldDesc>& GetComponentFields() {
    static const std::vector<ComponentFieldDesc> table = {
        Str("MeshComponent", "Mesh", &ComponentDecl::meshAssetPath,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).SetMeshPath(d.meshAssetPath); }),
        StrArr("MeshComponent", "Material", &ComponentDecl::materialAssetPaths,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).SetMaterialPaths(d.materialAssetPaths); }),
        Bln("MeshComponent", "MeshCastsShadow", &ComponentDecl::meshCastsShadow,
            [](Component& c, const ComponentDecl& d) { static_cast<MeshComponent&>(c).castsShadow = d.meshCastsShadow; }),

        Str("CollisionComponent", "CollisionShape", &ComponentDecl::collisionShape,
            [](Component& c, const ComponentDecl& d) {
                auto& col = static_cast<CollisionComponent&>(c);
                col.shapeType = d.collisionShape == "Sphere" ? CollisionComponent::Sphere
                              : d.collisionShape == "Capsule" ? CollisionComponent::Capsule
                              : CollisionComponent::Box;
            }),
        Flt("CollisionComponent", "CollisionRadius", &ComponentDecl::collisionRadius,
            [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).radius = d.collisionRadius; }),
        Flt("CollisionComponent", "CollisionHeight", &ComponentDecl::collisionHeight,
            [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).height = d.collisionHeight; }),

        Str("AudioComponent", "Sound", &ComponentDecl::soundAssetPath,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).soundPath = d.soundAssetPath; }),
        Bln("AudioComponent", "AutoPlay", &ComponentDecl::autoPlay,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).autoPlay = d.autoPlay; }),
        Bln("AudioComponent", "Looping", &ComponentDecl::looping,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).looping = d.looping; }),
        Flt("AudioComponent", "Volume", &ComponentDecl::volume,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).volume = d.volume; }),

        Flt("CameraComponent", "CameraFOV", &ComponentDecl::cameraFov,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).fov = d.cameraFov; }),
        Bln("CameraComponent", "CameraOrthographic", &ComponentDecl::cameraOrthographic,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).orthographic = d.cameraOrthographic; }),
        Flt("CameraComponent", "CameraNearClip", &ComponentDecl::cameraNearClip,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).nearClip = d.cameraNearClip; }),
        Flt("CameraComponent", "CameraFarClip", &ComponentDecl::cameraFarClip,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).farClip = d.cameraFarClip; }),

        Str("LightComponent", "LightType", &ComponentDecl::lightType,
            [](Component& c, const ComponentDecl& d) {
                auto& lc = static_cast<LightComponent&>(c);
                lc.lightType = d.lightType == "Rectangle" ? LightComponent::Rectangle
                              : d.lightType == "Spot"      ? LightComponent::Spot
                              : d.lightType == "Global"    ? LightComponent::Global
                              : d.lightType == "Sun"       ? LightComponent::Sun
                                                            : LightComponent::Point;
            }),
        Flt("LightComponent", "LightColorR", &ComponentDecl::lightColorR,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.x = d.lightColorR; }),
        Flt("LightComponent", "LightColorG", &ComponentDecl::lightColorG,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.y = d.lightColorG; }),
        Flt("LightComponent", "LightColorB", &ComponentDecl::lightColorB,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.z = d.lightColorB; }),
        Flt("LightComponent", "LightIntensity", &ComponentDecl::lightIntensity,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).intensity = d.lightIntensity; }),
        Flt("LightComponent", "LightRadius", &ComponentDecl::lightRadius,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).radius = d.lightRadius; }),
        Flt("LightComponent", "LightInnerConeAngle", &ComponentDecl::lightInnerConeAngle,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).innerConeAngle = d.lightInnerConeAngle; }),
        Flt("LightComponent", "LightOuterConeAngle", &ComponentDecl::lightOuterConeAngle,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).outerConeAngle = d.lightOuterConeAngle; }),
        Flt("LightComponent", "LightWidth", &ComponentDecl::lightWidth,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).width = d.lightWidth; }),
        Flt("LightComponent", "LightHeight", &ComponentDecl::lightHeight,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).height = d.lightHeight; }),

        StrArr("InputComponent", "InputMapping", &ComponentDecl::inputMappingNames,
            [](Component& c, const ComponentDecl& d) { static_cast<InputComponent&>(c).mappingNames = d.inputMappingNames; }),
        Str("InputComponent", "InputDevice", &ComponentDecl::inputDevice,
            [](Component& c, const ComponentDecl& d) { static_cast<InputComponent&>(c).device = d.inputDevice; }),
    };
    return table;
}

const std::vector<ComponentTypeDesc>& GetComponentTypes() {
    static const std::vector<ComponentTypeDesc> table = {
        { "MeshComponent",      []() -> Component* { return new MeshComponent(); } },
        { "CollisionComponent", []() -> Component* { return new CollisionComponent(); } },
        { "AudioComponent",     []() -> Component* { return new AudioComponent(); } },
        { "CameraComponent",    []() -> Component* { return new CameraComponent(); } },
        { "LightComponent",     []() -> Component* { return new LightComponent(); } },
        { "InputComponent",     []() -> Component* { return new InputComponent(); } },
    };
    return table;
}