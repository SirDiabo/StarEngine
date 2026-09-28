#pragma once
#include "SceneComponent.h"
#include "Runtime/CollisionTrace.h"

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

    static bool DeclVisibleInEditor(const ComponentDecl& decl);
    static bool BuildShapeFromDecl(const ComponentDecl& decl, const Vector3& pos, const Vector3& rot, const Vector3& scl,
                                   CollisionTrace::Shape& outShape);

    const char* GetClassName() const override { return "CollisionComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};

void RegisterCollisionComponentNodes();
