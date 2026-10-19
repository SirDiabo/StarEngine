#include "CollisionComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/ObjectPinRef.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "CollisionFilterFields.h"
#include "Modules/Stargraph/ObjectEvents.h"
#include "Runtime/Actor.h"
#include "Core/SafeParse.h"

void RegisterCollisionComponentNodes() {
    ObjectEvents::RegisterComponentEvent("CollisionComponent", { "Begin Overlap", { { "Other Actor", PinType::Object, "Actor" } } });
    ObjectEvents::RegisterComponentEvent("CollisionComponent", { "End Overlap", { { "Other Actor", PinType::Object, "Actor" } } });

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
    outProperties.AddBool("Visibility", &visible);
    outProperties.AddBool("Hidden In Game", &hiddenInGame);
    outProperties.AddBool("Is Trigger", &isTrigger);
}

bool CollisionComponent::DeclVisibleInEditor(const ComponentDecl& decl) {
    return decl.Get("CollisionVisibility", "true") == "true";
}

bool CollisionComponent::BuildShapeFromDecl(const ComponentDecl& decl, const Vector3& pos, const Vector3& rot, const Vector3& scl,
                                            CollisionTrace::Shape& outShape) {
    std::string shape = decl.Get("CollisionShape", "Box");
    int type = shape == "Sphere" ? Sphere : (shape == "Capsule" ? Capsule : Box);
    float radius = SafeParse::ToFloat(decl.Get("CollisionRadius", "50"));
    float height = SafeParse::ToFloat(decl.Get("CollisionHeight", "100"));
    Vector3 extents = { SafeParse::ToFloat(decl.Get("BoxWidth", "100")), SafeParse::ToFloat(decl.Get("BoxHeight", "100")),
                        SafeParse::ToFloat(decl.Get("BoxDepth", "100")) };
    return CollisionTrace::MakeComponentShape(type, radius, height, extents, pos, rot, scl, outShape);
}

void CollisionComponent::CollectTriggerOverlaps(std::vector<TriggerOverlapEntry>& out) const {
    out.clear();
    Actor* owner = GetOwningActor();
    if (!owner) return;
    CollisionTrace::Shape shape;
    if (!CollisionTrace::GetComponentShape(owner, this, shape)) return;
    CollisionTrace::QueryParams params;
    params.ignoreActor = owner;
    params.layer = collisionLayer;
    params.mask = collisionMask;
    params.triggers = CollisionTrace::TriggerMode::Include;
    std::vector<CollisionTrace::HitResult> hits;
    CollisionTrace::ScopedBatch batch;
    CollisionTrace::OverlapShapeAll(shape, params, hits);
    for (const CollisionTrace::HitResult& hit : hits) {
        if (!hit.actor || TriggerOverlap::Contains(out, hit.actor)) continue;
        TriggerOverlapEntry entry;
        entry.actor = hit.actor;
        entry.ref = ObjectPinRef::Encode(hit.actor->GetObjectKind(), hit.actor->GetObjectId());
        out.push_back(entry);
    }
}

void CollisionComponent::ProcessTriggerOverlaps(std::vector<TriggerOverlapEntry>& began, std::vector<TriggerOverlapEntry>& ended) {
    std::vector<TriggerOverlapEntry> current;
    if (isTrigger) CollectTriggerOverlaps(current);
    TriggerOverlap::Diff(overlapping, current, began, ended);
    overlapping = std::move(current);
}

void CollisionComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
    if (!isTrigger && overlapping.empty()) return;
    std::vector<TriggerOverlapEntry> began, ended;
    ProcessTriggerOverlaps(began, ended);
    for (const TriggerOverlapEntry& e : ended) ObjectEvents::RaiseComponentEvent(this, "End Overlap", { { "Other Actor", e.ref } });
    for (const TriggerOverlapEntry& e : began) ObjectEvents::RaiseComponentEvent(this, "Begin Overlap", { { "Other Actor", e.ref } });
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
            },
            [](const Component& c) -> std::string {
                switch (static_cast<const CollisionComponent&>(c).shapeType) {
                    case CollisionComponent::Sphere:  return "Sphere";
                    case CollisionComponent::Capsule: return "Capsule";
                    default:                          return "Box";
                }
            }),
        []{ auto f = ComponentField::Flt("CollisionRadius", "Radius", 50.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).radius = SafeParse::ToFloat(d.Get("CollisionRadius", "50")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CollisionComponent&>(c).radius); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") != "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("CollisionHeight", "Height", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).height = SafeParse::ToFloat(d.Get("CollisionHeight", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CollisionComponent&>(c).height); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Capsule"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxWidth", "Box Width", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.x = SafeParse::ToFloat(d.Get("BoxWidth", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CollisionComponent&>(c).boxExtents.x); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxHeight", "Box Height", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.y = SafeParse::ToFloat(d.Get("BoxHeight", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CollisionComponent&>(c).boxExtents.y); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
        []{ auto f = ComponentField::Flt("BoxDepth", "Box Depth", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).boxExtents.z = SafeParse::ToFloat(d.Get("BoxDepth", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CollisionComponent&>(c).boxExtents.z); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionShape", "Box") == "Box"; };
            return f; }(),
        ComponentField::Bln("CollisionVisibility", "Visibility", true,
            [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).visible = d.Get("CollisionVisibility", "true") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const CollisionComponent&>(c).visible); }),
        []{ auto f = ComponentField::Bln("CollisionHiddenInGame", "Hidden In Game", true,
                [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).hiddenInGame = d.Get("CollisionHiddenInGame", "true") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const CollisionComponent&>(c).hiddenInGame); });
            f.visibleIf = [](const ComponentDecl& d) { return d.Get("CollisionVisibility", "true") == "true"; };
            return f; }(),
        ComponentField::Bln("IsTrigger", "Is Trigger", false,
            [](Component& c, const ComponentDecl& d) { static_cast<CollisionComponent&>(c).isTrigger = d.Get("IsTrigger", "false") == "true"; },
            [](const Component& c) -> std::string { return static_cast<const CollisionComponent&>(c).isTrigger ? "true" : "false"; }),
        CollisionFilterFields::Layer<CollisionComponent>(),
        CollisionFilterFields::Mask<CollisionComponent>(),
    }
});
}
