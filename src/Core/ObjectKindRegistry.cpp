#include "ObjectKindRegistry.h"

ObjectKindRegistry& ObjectKindRegistry::Get() {
    static ObjectKindRegistry instance;
    return instance;
}

ObjectKindRegistry::ObjectKindRegistry() {
    RegisterBuiltInKinds();
}

void ObjectKindRegistry::RegisterBuiltInKinds() {
    RegisterKind("Actor", "");
    RegisterKind("Component", "");
    RegisterKind("SceneComponent", "Component");
    RegisterKind("CameraComponent", "SceneComponent");
    RegisterKind("AudioComponent", "SceneComponent");
    RegisterKind("MeshComponent", "SceneComponent");
    RegisterKind("CollisionComponent", "SceneComponent");
    RegisterKind("InputComponent", "SceneComponent");
    RegisterKind("PhysicsComponent", "SceneComponent");
    RegisterKind("LightComponent", "SceneComponent");
    RegisterKind("BillboardComponent", "SceneComponent");
    RegisterKind("WidgetComponent", "SceneComponent");
    RegisterKind("Widget", "");
    RegisterKind("WidgetElement", "");
    RegisterKind("BoxElement", "WidgetElement");
    RegisterKind("VerticalBoxElement", "BoxElement");
    RegisterKind("HorizontalBoxElement", "BoxElement");
    RegisterKind("GridElement", "WidgetElement");
    RegisterKind("RectangleElement", "WidgetElement");
    RegisterKind("ButtonElement", "RectangleElement");
    RegisterKind("TabHeaderButtonElement", "ButtonElement");
    RegisterKind("TextElement", "WidgetElement");
    RegisterKind("ImageElement", "WidgetElement");
    RegisterKind("ProgressBarElement", "WidgetElement");
    RegisterKind("SpacerElement", "WidgetElement");
    RegisterKind("SplitBoxElement", "WidgetElement");
    RegisterKind("TabContainer", "WidgetElement");
    RegisterKind("CheckBoxElement", "WidgetElement");
    RegisterKind("SliderElement", "WidgetElement");
    RegisterKind("SlotElement", "WidgetElement");
    RegisterKind("ColorPickerElement", "WidgetElement");
    RegisterKind("WidgetObjectElement", "WidgetElement");
    RegisterKind("SearchListElement", "WidgetElement");
    RegisterKind("TreeListElement", "WidgetElement");
    RegisterKind("Sound", "");
    RegisterKind("Music", "");
    RegisterKind("Texture", "");
    RegisterKind("MeshActor", "Actor");
    RegisterKind("PointLightActor", "Actor");
    RegisterKind("SunLightActor", "Actor");
    RegisterKind("SpotLightActor", "Actor");
    RegisterKind("RectLightActor", "Actor");
    RegisterKind("GlobalLightActor", "Actor");
    RegisterKind("CameraActor", "Actor");
    RegisterKind("AudioActor", "Actor");
}

void ObjectKindRegistry::RegisterKind(const std::string& kind, const std::string& parentKind) {
    parents[kind] = parentKind;
}

std::string ObjectKindRegistry::GetParentKind(const std::string& kind) const {
    auto it = parents.find(kind);
    return it != parents.end() ? it->second : std::string();
}

bool ObjectKindRegistry::IsCompatible(const std::string& candidateKind, const std::string& requiredKind) const {
    if (candidateKind.empty() || requiredKind.empty() || candidateKind == requiredKind) return true;

    std::string current = candidateKind;
    for (int guard = 0; guard < 32; guard++) {
        auto it = parents.find(current);
        if (it == parents.end() || it->second.empty()) return false;
        current = it->second;
        if (current == requiredKind) return true;
    }
    return false;
}
