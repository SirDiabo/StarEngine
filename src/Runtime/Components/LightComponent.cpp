#include "LightComponent.h"

void LightComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddVector3("Light Color", &lightColor.x);
    outProperties.AddFloat("Intensity", &intensity);
    outProperties.AddFloat("Radius", &radius);
    outProperties.AddFloat("Inner Cone Angle", &innerConeAngle);
    outProperties.AddFloat("Outer Cone Angle", &outerConeAngle);
    outProperties.AddFloat("Width", &width);
    outProperties.AddFloat("Height", &height);
}

void LightComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}
