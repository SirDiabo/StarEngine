#include "CollisionComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include <cstdlib>

void RegisterCollisionComponentNodes() {
    NodeTemplate setCollisionRadius;
    setCollisionRadius.type = "Set_Collision_Radius";
    setCollisionRadius.category = "Collision";
    setCollisionRadius.moduleName = "Stargraph";
    setCollisionRadius.hideFromContextMenu = true;
    setCollisionRadius.requiresComponentClass = "CollisionComponent";
    setCollisionRadius.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Radius", PinType::Float, true, false, 0, "50.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCollisionRadius.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float radius = 50.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Radius" && !p.defaultValue.empty()) radius = (float)atof(p.defaultValue.c_str());
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
    setCollisionHeight.requiresComponentClass = "CollisionComponent";
    setCollisionHeight.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Height", PinType::Float, true, false, 0, "100.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCollisionHeight.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float height = 100.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Height" && !p.defaultValue.empty()) height = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        if (CollisionComponent* cc = dynamic_cast<CollisionComponent*>(target)) {
            cc->height = height;
        }
    };
    NodeRegistry::Get().RegisterNode(setCollisionHeight);
}

void CollisionComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("Radius", &radius);
    outProperties.AddFloat("Height", &height);
}

void CollisionComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}