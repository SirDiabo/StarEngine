#include "ComponentFields.h"

namespace {

ComponentFieldDesc Str(const char* type, const char* key, std::string ComponentDecl::* field) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::String;
    d.stringField = field;
    return d;
}

ComponentFieldDesc Flt(const char* type, const char* key, float ComponentDecl::* field) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::Float;
    d.floatField = field;
    return d;
}

ComponentFieldDesc Bln(const char* type, const char* key, bool ComponentDecl::* field) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::Bool;
    d.boolField = field;
    return d;
}

ComponentFieldDesc StrArr(const char* type, const char* key, std::vector<std::string> ComponentDecl::* field) {
    ComponentFieldDesc d;
    d.componentType = type;
    d.key = key;
    d.type = ComponentFieldType::StringArray;
    d.stringArrayField = field;
    return d;
}

} // namespace

const std::vector<ComponentFieldDesc>& GetComponentFields() {
    static const std::vector<ComponentFieldDesc> table = {
        Str("MeshComponent", "Mesh", &ComponentDecl::meshAssetPath),
        StrArr("MeshComponent", "Material", &ComponentDecl::materialAssetPaths),

        Str("CollisionComponent", "CollisionShape", &ComponentDecl::collisionShape),
        Flt("CollisionComponent", "CollisionRadius", &ComponentDecl::collisionRadius),
        Flt("CollisionComponent", "CollisionHeight", &ComponentDecl::collisionHeight),

        Str("AudioComponent", "Sound", &ComponentDecl::soundAssetPath),
        Bln("AudioComponent", "AutoPlay", &ComponentDecl::autoPlay),
        Bln("AudioComponent", "Looping", &ComponentDecl::looping),
        Flt("AudioComponent", "Volume", &ComponentDecl::volume),

        Flt("CameraComponent", "CameraFOV", &ComponentDecl::cameraFov),
        Bln("CameraComponent", "CameraOrthographic", &ComponentDecl::cameraOrthographic),

        StrArr("InputComponent", "InputMapping", &ComponentDecl::inputMappingNames),
        Str("InputComponent", "InputDevice", &ComponentDecl::inputDevice),
    };
    return table;
}
