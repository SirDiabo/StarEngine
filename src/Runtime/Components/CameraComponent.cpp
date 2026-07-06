#include "CameraComponent.h"

CameraComponent::CameraComponent() : SceneComponent() {
}

void CameraComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("FOV", &fov);
}

void CameraComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}
