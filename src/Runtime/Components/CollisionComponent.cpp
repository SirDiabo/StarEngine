#include "CollisionComponent.h"

void CollisionComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("Radius", &radius);
    outProperties.AddFloat("Height", &height);
}

void CollisionComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}