#include "CameraComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"

void RegisterCameraComponentNodes() {
    NodeTemplate setCameraFov;
    setCameraFov.type = "Set_Camera_FOV";
    setCameraFov.category = "Camera";
    setCameraFov.moduleName = "";
    setCameraFov.hideFromContextMenu = false;
    setCameraFov.requiresComponentClass = "CameraComponent";
    setCameraFov.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"FOV", PinType::Float, true, false, 0, "50.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCameraFov.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        float fov = 50.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "FOV" && !p.defaultValue.empty()) fov = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        if (CameraComponent* cc = dynamic_cast<CameraComponent*>(target)) {
            cc->fov = fov;
        }
    };
    NodeRegistry::Get().RegisterNode(setCameraFov);
}

CameraComponent::CameraComponent() : SceneComponent() {
}

void CameraComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("FOV", &fov);
}

void CameraComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}
