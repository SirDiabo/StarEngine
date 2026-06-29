#pragma once
#include "SceneComponent.h"

class CollisionComponent : public SceneComponent {
public:
    enum ShapeType { Box, Sphere, Capsule };
    ShapeType shapeType = Box;
    float radius = 50.0f;
    float height = 100.0f;

    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};