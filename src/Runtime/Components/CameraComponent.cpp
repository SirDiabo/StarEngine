#include "CameraComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "raylib.h"
#include "raymath.h"

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

    NodeTemplate getCameraMatrix;
    getCameraMatrix.type = "Get_Camera_Matrix";
    getCameraMatrix.category = "Camera";
    getCameraMatrix.moduleName = "Stargraph";
    getCameraMatrix.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"View Matrix", PinType::Vec3, false, true});
        node->outputs.push_back({"Projection Matrix", PinType::Vec3, false, true});
    };
    getCameraMatrix.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* targetActor = World::Get().GetViewTarget();
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                targetActor = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!targetActor) {
            targetActor = ctx.actor;
        }
        if (!targetActor) return;

        Camera3D cam = {0};
        cam.up = {0, 1, 0};
        if (targetActor->GetCameraComponent()) {
            CameraComponent* cc = targetActor->GetCameraComponent();
            Vector3 compPos = Actor::ComputeWorldPosition(cc->position, targetActor->position, targetActor->rotation, targetActor->scale);
            Vector3 compRot = Actor::ComputeWorldRotation(cc->rotation, targetActor->rotation);
            float yr = compRot.y * DEG2RAD;
            float pr = compRot.x * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            cam.position = compPos;
            cam.target = {compPos.x + forward.x, compPos.y + forward.y, compPos.z + forward.z};
            cam.fovy = cc->fov;
            cam.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
        } else {
            Vector3 forward = {cosf(targetActor->rotation.y * DEG2RAD), 0, sinf(targetActor->rotation.y * DEG2RAD)};
            cam.position = targetActor->position;
            cam.target = {targetActor->position.x + forward.x, targetActor->position.y + forward.y, targetActor->position.z + forward.z};
            cam.fovy = 50.0f;
            cam.projection = CAMERA_PERSPECTIVE;
        }

        Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
        Matrix proj = MatrixIdentity();
        if (cam.projection == CAMERA_PERSPECTIVE) {
            proj = MatrixPerspective(cam.fovy * DEG2RAD, (float)GetScreenWidth() / (float)GetScreenHeight(), 0.1f, 1000.0f);
        } else {
            proj = MatrixOrtho(0, (float)GetScreenWidth(), 0, (float)GetScreenHeight(), -1000.0f, 1000.0f);
        }

        char viewBuf[512], projBuf[512];
        snprintf(viewBuf, sizeof(viewBuf), "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
            view.m0, view.m1, view.m2, view.m3, view.m4, view.m5, view.m6, view.m7, view.m8, view.m9, view.m10, view.m11, view.m12, view.m13, view.m14, view.m15);
        snprintf(projBuf, sizeof(projBuf), "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
            proj.m0, proj.m1, proj.m2, proj.m3, proj.m4, proj.m5, proj.m6, proj.m7, proj.m8, proj.m9, proj.m10, proj.m11, proj.m12, proj.m13, proj.m14, proj.m15);

        for (Pin& p : node.outputs) {
            if (p.name == "View Matrix") p.defaultValue = viewBuf;
            if (p.name == "Projection Matrix") p.defaultValue = projBuf;
        }
    };
    NodeRegistry::Get().RegisterNode(getCameraMatrix);

    NodeTemplate updateCamera;
    updateCamera.type = "Update_Camera";
    updateCamera.category = "Camera";
    updateCamera.moduleName = "Stargraph";
    updateCamera.hideFromContextMenu = true;
    updateCamera.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    updateCamera.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* targetActor = World::Get().GetViewTarget();
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                targetActor = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!targetActor) {
            targetActor = ctx.actor;
        }
        if (!targetActor) return;
        World::Get().SetViewTarget(targetActor);
    };
    NodeRegistry::Get().RegisterNode(updateCamera);

    NodeTemplate cameraToTarget;
    cameraToTarget.type = "Camera_To_Target";
    cameraToTarget.category = "Camera";
    cameraToTarget.moduleName = "Stargraph";
    cameraToTarget.hideFromContextMenu = true;
    cameraToTarget.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    cameraToTarget.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* targetActor = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                targetActor = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!targetActor) return;
        World::Get().SetViewTarget(targetActor);
    };
    NodeRegistry::Get().RegisterNode(cameraToTarget);
}

CameraComponent::CameraComponent() : SceneComponent() {
}

void CameraComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("FOV", &fov);
    outProperties.AddBool("Orthographic", &orthographic);
    outProperties.AddBool("Use Control Yaw", &useControlYaw);
    outProperties.AddBool("Use Control Pitch", &useControlPitch);
    outProperties.AddBool("Use Control Roll", &useControlRoll);
}

void CameraComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}