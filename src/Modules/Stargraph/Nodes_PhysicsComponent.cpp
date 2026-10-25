#include "Runtime/Components/PhysicsComponent.h"
#include "Runtime/Actor.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "ExecutionContext.h"
#include "ObjectPinRef.h"
#include "Core/SafeParse.h"
#include "Nodes_Components.h"
#include <cstdio>
#include <string>

namespace {

PhysicsComponent* ResolvePhysics(Node& node, ExecutionContext& ctx) {
    for (const Pin& p : node.inputs) {
        if (p.name == "Target" && !p.defaultValue.empty()) {
            Component* c = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "PhysicsComponent", ctx);
            return dynamic_cast<PhysicsComponent*>(c);
        }
    }
    return ctx.actor ? ctx.actor->GetPhysicsComponent() : nullptr;
}

std::string FormatVec3(const Vector3& v) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", v.x, v.y, v.z);
    return buf;
}

void SetOutput(Node& node, const char* name, const std::string& value) {
    for (Pin& p : node.outputs) if (p.name == name) p.defaultValue = value;
}

}

void RegisterPhysicsComponentNodes() {
    NodeTemplate setVelocity;
    setVelocity.type = "Set_Velocity";
    setVelocity.category = "Physics";
    setVelocity.moduleName = "Stargraph";
    setVelocity.hideFromContextMenu = true;
    setVelocity.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->inputs.push_back({"Velocity", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVelocity.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Velocity") {
                Vector3 v;
                ReadVec3(p, v.x, v.y, v.z);
                target->SetVelocity(v);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setVelocity);

    NodeTemplate addImpulse;
    addImpulse.type = "Add_Impulse";
    addImpulse.category = "Physics";
    addImpulse.moduleName = "Stargraph";
    addImpulse.hideFromContextMenu = true;
    addImpulse.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->inputs.push_back({"Impulse", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    addImpulse.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Impulse") {
                Vector3 v;
                ReadVec3(p, v.x, v.y, v.z);
                target->AddImpulse(v);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(addImpulse);

    NodeTemplate setSimulate;
    setSimulate.type = "Set_Simulate_Physics";
    setSimulate.category = "Physics";
    setSimulate.moduleName = "Stargraph";
    setSimulate.hideFromContextMenu = true;
    setSimulate.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->inputs.push_back({"Simulate", PinType::Boolean, true, false, 0, "true"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSimulate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Simulate") target->simulatePhysics = ReadBool(p);
        }
    };
    NodeRegistry::Get().RegisterNode(setSimulate);

    NodeTemplate getVelocity;
    getVelocity.type = "Get_Velocity";
    getVelocity.category = "Physics";
    getVelocity.moduleName = "Stargraph";
    getVelocity.hideFromContextMenu = true;
    getVelocity.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->outputs.push_back({"Velocity", PinType::Vec3, false, true});
    };
    getVelocity.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        SetOutput(node, "Velocity", target ? FormatVec3(target->velocity) : "0,0,0");
    };
    NodeRegistry::Get().RegisterNode(getVelocity);

    NodeTemplate isGrounded;
    isGrounded.type = "Is_Grounded";
    isGrounded.category = "Physics";
    isGrounded.moduleName = "Stargraph";
    isGrounded.hideFromContextMenu = true;
    isGrounded.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->outputs.push_back({"Grounded", PinType::Boolean, false, true});
    };
    isGrounded.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        SetOutput(node, "Grounded", target && target->grounded ? "true" : "false");
    };
    NodeRegistry::Get().RegisterNode(isGrounded);

    NodeTemplate getGroundNormal;
    getGroundNormal.type = "Get_Ground_Normal";
    getGroundNormal.category = "Physics";
    getGroundNormal.moduleName = "Stargraph";
    getGroundNormal.hideFromContextMenu = true;
    getGroundNormal.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "PhysicsComponent"});
        node->outputs.push_back({"Normal", PinType::Vec3, false, true});
    };
    getGroundNormal.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PhysicsComponent* target = ResolvePhysics(node, ctx);
        SetOutput(node, "Normal", target ? FormatVec3(target->groundNormal) : "0,1,0");
    };
    NodeRegistry::Get().RegisterNode(getGroundNormal);
}
