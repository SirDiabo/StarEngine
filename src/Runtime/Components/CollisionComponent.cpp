#include "CollisionComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/ObjectPinRef.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/SafeParse.h"

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

void CollisionComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("Radius", &radius);
    outProperties.AddFloat("Height", &height);
    outProperties.AddVector3("Box Extents", &boxExtents.x);
}

void CollisionComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

namespace {
ComponentRegistrar registrar_CollisionComponent({
    "CollisionComponent",
    []() -> Component* { return new CollisionComponent(); },
    {
        ComponentField::Enm("CollisionShape", "Shape", {"Box", "Sphere", "Capsule"}, "Box",
            [](Component& c, const ComponentDecl& d) {
                std::string shape = d.Get("CollisionShape", "Box");
                static_cast<CollisionComponent&>(c).shapeType = shape == "Sphere" ? CollisionComponent::Sphere
                                                                : shape == "Capsule" ? CollisionComponent::Capsule
                                                                                     : CollisionComponent::Box;
            }),
        []{ auto f = ComponentField::Flt("CollisionRadius", "Radius", 50.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).radius = SafeParse::ToFloat(d.Get("CollisionRadius", "50")); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") != "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("CollisionHeight", "Height", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).height = SafeParse::ToFloat(d.Get("CollisionHeight", "100")); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Capsule"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxWidth", "Box Width", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.x = SafeParse::ToFloat(d.Get("BoxWidth", "100")); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxHeight", "Box Height", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.y = SafeParse::ToFloat(d.Get("BoxHeight", "100")); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxDepth", "Box Depth", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.z = SafeParse::ToFloat(d.Get("BoxDepth", "100")); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
    }
});
}
