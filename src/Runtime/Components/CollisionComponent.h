#pragma once
#include "SceneComponent.h"
#include "Runtime/CollisionTrace.h"
#include "Runtime/TriggerOverlap.h"
#include <cstdint>
#include <vector>

struct ComponentDecl;

class CollisionComponent : public SceneComponent {
public:
    enum ShapeType { Box, Sphere, Capsule };
    ShapeType shapeType = Box;
    float radius = 50.0f;
    float height = 100.0f;
    Vector3 boxExtents = {100.0f, 100.0f, 100.0f};
    bool visible = true;
    bool hiddenInGame = true;
    int collisionLayer = 0;
    uint32_t collisionMask = 0xFFFFFFFFu;
    bool isTrigger = false;

    static bool DeclVisibleInEditor(const ComponentDecl& decl);
    static bool BuildShapeFromDecl(const ComponentDecl& decl, const Vector3& pos, const Vector3& rot, const Vector3& scl,
                                   CollisionTrace::Shape& outShape);

    const char* GetClassName() const override { return "CollisionComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;

    void CollectTriggerOverlaps(std::vector<TriggerOverlapEntry>& out) const;
    void ProcessTriggerOverlaps(std::vector<TriggerOverlapEntry>& began, std::vector<TriggerOverlapEntry>& ended);
    const std::vector<TriggerOverlapEntry>& GetTriggerOverlaps() const { return overlapping; }

private:
    std::vector<TriggerOverlapEntry> overlapping;
};

void RegisterCollisionComponentNodes();
