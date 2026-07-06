#include "CameraComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"

void RegisterCameraComponentNodes() {
    NodeTemplate setCameraFov;
    setCameraFov.type = "Set_Camera_FOV";
    setCameraFov.category = "Camera";
    setCameraFov.moduleName = "Stargraph";
    setCameraFov.hideFromContextMenu = true;
    setCameraFov.requiresComponentClass = "CameraComponent";
    setCameraFov.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"FOV", PinType::Float, true, false, 0, "50.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCameraFov.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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

    // Get Camera FOV
    NodeTemplate getCameraFov;
    getCameraFov.type = "Get_Camera_FOV";
    getCameraFov.category = "Camera";
    getCameraFov.moduleName = "Stargraph";
    getCameraFov.hideFromContextMenu = true;
    getCameraFov.requiresComponentClass = "CameraComponent";
    getCameraFov.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"FOV", PinType::Float, false, true});
    };
    getCameraFov.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        if (CameraComponent* cc = dynamic_cast<CameraComponent*>(target)) {
            char buf[32]; snprintf(buf, sizeof(buf), "%g", cc->fov);
            for (Pin& p : node.outputs) if (p.name == "FOV") p.defaultValue = buf;
        }
    };
    NodeRegistry::Get().RegisterNode(getCameraFov);

    // Set Camera Orthographic
    NodeTemplate setCameraOrtho;
    setCameraOrtho.type = "Set_Camera_Orthographic";
    setCameraOrtho.category = "Camera";
    setCameraOrtho.moduleName = "Stargraph";
    setCameraOrtho.hideFromContextMenu = true;
    setCameraOrtho.requiresComponentClass = "CameraComponent";
    setCameraOrtho.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Orthographic", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCameraOrtho.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        bool ortho = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Orthographic") ortho = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        if (!target) return;
        if (CameraComponent* cc = dynamic_cast<CameraComponent*>(target)) {
            cc->orthographic = ortho;
        }
    };
    NodeRegistry::Get().RegisterNode(setCameraOrtho);

    // Get Camera Orthographic
    NodeTemplate getCameraOrtho;
    getCameraOrtho.type = "Get_Camera_Orthographic";
    getCameraOrtho.category = "Camera";
    getCameraOrtho.moduleName = "Stargraph";
    getCameraOrtho.hideFromContextMenu = true;
    getCameraOrtho.requiresComponentClass = "CameraComponent";
    getCameraOrtho.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Orthographic", PinType::Boolean, false, true});
    };
    getCameraOrtho.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        if (CameraComponent* cc = dynamic_cast<CameraComponent*>(target)) {
            for (Pin& p : node.outputs) if (p.name == "Orthographic") p.defaultValue = cc->orthographic ? "true" : "false";
        }
    };
    NodeRegistry::Get().RegisterNode(getCameraOrtho);
}

CameraComponent::CameraComponent() : SceneComponent() {
}

void CameraComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("FOV", &fov);
    outProperties.AddBool("Orthographic", &orthographic);
}

void CameraComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}
