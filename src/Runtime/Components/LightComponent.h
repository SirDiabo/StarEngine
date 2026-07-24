#pragma once
#include "SceneComponent.h"

class LightComponent : public SceneComponent {
public:
    enum LightType { Point, Rectangle, Spot, Global, Sun };
    LightType lightType = Point;

    Vector3 lightColor{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    // Point, Rectangle, Spot only.
    float radius = 500.0f;

    // Spot only, degrees.
    float innerConeAngle = 25.0f;
    float outerConeAngle = 35.0f;

    // Rectangle only.
    float width = 100.0f;
    float height = 100.0f;

    const char* GetClassName() const override { return "LightComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};
