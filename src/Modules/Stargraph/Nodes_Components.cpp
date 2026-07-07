#include "Nodes_Components.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/Component.h"
#include "raymath.h"
#include <cstdlib>

void RegisterComponentNodes() {
    NodeTemplate getComponent;
    getComponent.type = "Get_Component";
    getComponent.category = "Components";
    getComponent.moduleName = "Stargraph";
    getComponent.hideFromContextMenu = true;
    getComponent.createPins = [](Node* node) {
        node->inputs.push_back({"Component_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Component", PinType::Object, false, true});
    };
    getComponent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string compName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Component_Name") { compName = p.defaultValue; break; }
        }
        if (!ctx.actor || compName.empty()) return;
        Component* comp = ctx.actor->FindComponentByName(compName);
        if (!comp) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%p", (void*)comp);
        for (Pin& p : node.outputs) {
            if (p.name == "Component") { p.defaultValue = buf; break; }
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Location", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 loc = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Location") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &loc.x, &loc.y, &loc.z);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Rotation", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 rot = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Rotation") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &rot.x, &rot.y, &rot.z);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Scale", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 scl = {1, 1, 1};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Scale") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &scl.x, &scl.y, &scl.z);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompWorldLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        Vector3 worldLoc = Actor::ComputeWorldPosition(target->position, ctx.actor->position, ctx.actor->rotation, ctx.actor->scale);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", worldLoc.x, worldLoc.y, worldLoc.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompWorldLocation);
}