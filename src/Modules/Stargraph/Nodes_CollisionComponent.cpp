#include "Runtime/Components/CollisionComponent.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "ObjectPinRef.h"
#include "ObjectEvents.h"
#include "Runtime/Actor.h"
#include "Core/SafeParse.h"
#include "Nodes_Components.h"

void RegisterCollisionComponentNodes() {
    NodeTemplate setCollisionRadius;
    setCollisionRadius.type = "Set_Collision_Radius";
    setCollisionRadius.category = "Collision";
    setCollisionRadius.moduleName = "Stargraph";
    setCollisionRadius.hideFromContextMenu = true;
    setCollisionRadius.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "CollisionComponent"});
        node->inputs.push_back({"Radius", PinType::Float, true, false, 0, "50.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCollisionRadius.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float radius = 50.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "CollisionComponent", ctx);
            }
            if (p.name == "Radius" && !p.defaultValue.empty()) radius = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (CollisionComponent* cc = dynamic_cast<CollisionComponent*>(target)) {
            cc->radius = radius;
        }
    };
    NodeRegistry::Get().RegisterNode(setCollisionRadius);

    NodeTemplate setCollisionHeight;
    setCollisionHeight.type = "Set_Collision_Height";
    setCollisionHeight.category = "Collision";
    setCollisionHeight.moduleName = "Stargraph";
    setCollisionHeight.hideFromContextMenu = true;
    setCollisionHeight.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "CollisionComponent"});
        node->inputs.push_back({"Height", PinType::Float, true, false, 0, "100.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCollisionHeight.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float height = 100.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "CollisionComponent", ctx);
            }
            if (p.name == "Height" && !p.defaultValue.empty()) height = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (CollisionComponent* cc = dynamic_cast<CollisionComponent*>(target)) {
            cc->height = height;
        }
    };
    NodeRegistry::Get().RegisterNode(setCollisionHeight);
}
