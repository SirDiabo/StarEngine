#include "Nodes_Components.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Components/MeshComponent.h"
#include "raymath.h"
#include <cstdlib>
#include "ObjectPinRef.h"

void RegisterComponentNodes() {
    NodeTemplate getComponent;
    getComponent.type = "Get_Component";
    getComponent.category = "Components";
    getComponent.moduleName = "Stargraph";
    getComponent.hideFromContextMenu = true;
    getComponent.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Component_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Component", PinType::Object, false, true, 0, "", false, "Component"});
    };
    getComponent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string compName;
        Actor* targetActor = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Component_Name") compName = p.defaultValue;
            if (p.name == "Target" && !p.defaultValue.empty()) targetActor = ObjectPinRef::ResolveAs<Actor>(node, p.defaultValue, "Actor", ctx);
        }
        if (!targetActor || compName.empty()) return;
        Component* comp = targetActor->FindComponentByName(compName);
        if (!comp) return;
        for (Pin& p : node.outputs) {
            if (p.name == "Component") { p.defaultValue = ObjectPinRef::Encode(comp->GetObjectKind(), comp->GetObjectId()); break; }
        }
    };
    NodeRegistry::Get().RegisterNode(getComponent);

    NodeTemplate setCompLocation;
    setCompLocation.type = "Set_Relative_Location";
    setCompLocation.category = "Components";
    setCompLocation.moduleName = "Stargraph";
    setCompLocation.hideFromContextMenu = true;
    setCompLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Location", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 loc = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Location") ReadVec3(p, loc.x, loc.y, loc.z);
        }
        if (!target) return;
        target->position = loc;
    };
    NodeRegistry::Get().RegisterNode(setCompLocation);

    NodeTemplate setCompRotation;
    setCompRotation.type = "Set_Relative_Rotation";
    setCompRotation.category = "Components";
    setCompRotation.moduleName = "Stargraph";
    setCompRotation.hideFromContextMenu = true;
    setCompRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Rotation", PinType::Rotator, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 rot = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Rotation") ReadVec3(p, rot.x, rot.y, rot.z);
        }
        if (!target) return;
        target->rotation = rot;
    };
    NodeRegistry::Get().RegisterNode(setCompRotation);

    NodeTemplate setCompScale;
    setCompScale.type = "Set_Relative_Scale";
    setCompScale.category = "Components";
    setCompScale.moduleName = "Stargraph";
    setCompScale.hideFromContextMenu = true;
    setCompScale.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Scale", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 scl = {1, 1, 1};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Scale") ReadVec3(p, scl.x, scl.y, scl.z);
        }
        if (!target) return;
        target->scale = scl;
    };
    NodeRegistry::Get().RegisterNode(setCompScale);

    NodeTemplate getCompLocation;
    getCompLocation.type = "Get_Relative_Location";
    getCompLocation.category = "Components";
    getCompLocation.moduleName = "Stargraph";
    getCompLocation.hideFromContextMenu = true;
    getCompLocation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->position.x, target->position.y, target->position.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompLocation);

    NodeTemplate getCompRotation;
    getCompRotation.type = "Get_Relative_Rotation";
    getCompRotation.category = "Components";
    getCompRotation.moduleName = "Stargraph";
    getCompRotation.hideFromContextMenu = true;
    getCompRotation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Rotation", PinType::Rotator, false, true});
    };
    getCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->rotation.x, target->rotation.y, target->rotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompRotation);

    NodeTemplate getCompScale;
    getCompScale.type = "Get_Relative_Scale";
    getCompScale.category = "Components";
    getCompScale.moduleName = "Stargraph";
    getCompScale.hideFromContextMenu = true;
    getCompScale.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->scale.x, target->scale.y, target->scale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompScale);

    NodeTemplate getCompWorldLocation;
    getCompWorldLocation.type = "Get_World_Location";
    getCompWorldLocation.category = "Components";
    getCompWorldLocation.moduleName = "Stargraph";
    getCompWorldLocation.hideFromContextMenu = true;
    getCompWorldLocation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompWorldLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        Vector3 worldLoc = ctx.actor->GetComponentWorldPosition(target);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", worldLoc.x, worldLoc.y, worldLoc.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompWorldLocation);

    NodeTemplate getCompWorldRotation;
    getCompWorldRotation.type = "Get_World_Rotation";
    getCompWorldRotation.category = "Components";
    getCompWorldRotation.moduleName = "Stargraph";
    getCompWorldRotation.hideFromContextMenu = true;
    getCompWorldRotation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Rotation", PinType::Rotator, false, true});
    };
    getCompWorldRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        Vector3 worldRot = ctx.actor->GetComponentWorldRotation(target);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", worldRot.x, worldRot.y, worldRot.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompWorldRotation);

    NodeTemplate getCompWorldScale;
    getCompWorldScale.type = "Get_World_Scale";
    getCompWorldScale.category = "Components";
    getCompWorldScale.moduleName = "Stargraph";
    getCompWorldScale.hideFromContextMenu = true;
    getCompWorldScale.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getCompWorldScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
        }
        if (!target) return;
        Vector3 worldScale = ctx.actor->GetComponentWorldScale(target);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", worldScale.x, worldScale.y, worldScale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompWorldScale);

    NodeTemplate setCompWorldLocation;
    setCompWorldLocation.type = "Set_World_Location";
    setCompWorldLocation.category = "Components";
    setCompWorldLocation.moduleName = "Stargraph";
    setCompWorldLocation.hideFromContextMenu = true;
    setCompWorldLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Location", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompWorldLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 loc = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Location") ReadVec3(p, loc.x, loc.y, loc.z);
        }
        if (!target) return;
        ctx.actor->SetComponentWorldPosition(target, loc);
    };
    NodeRegistry::Get().RegisterNode(setCompWorldLocation);

    NodeTemplate setCompWorldRotation;
    setCompWorldRotation.type = "Set_World_Rotation";
    setCompWorldRotation.category = "Components";
    setCompWorldRotation.moduleName = "Stargraph";
    setCompWorldRotation.hideFromContextMenu = true;
    setCompWorldRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Rotation", PinType::Rotator, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompWorldRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 rot = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Rotation") ReadVec3(p, rot.x, rot.y, rot.z);
        }
        if (!target) return;
        ctx.actor->SetComponentWorldRotation(target, rot);
    };
    NodeRegistry::Get().RegisterNode(setCompWorldRotation);

    NodeTemplate setCompWorldScale;
    setCompWorldScale.type = "Set_World_Scale";
    setCompWorldScale.category = "Components";
    setCompWorldScale.moduleName = "Stargraph";
    setCompWorldScale.hideFromContextMenu = true;
    setCompWorldScale.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Component"});
        node->inputs.push_back({"Scale", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompWorldScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 scl = {1, 1, 1};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "Component", ctx);
            }
            if (p.name == "Scale") ReadVec3(p, scl.x, scl.y, scl.z);
        }
        if (!target) return;
        ctx.actor->SetComponentWorldScale(target, scl);
    };
    NodeRegistry::Get().RegisterNode(setCompWorldScale);
}
