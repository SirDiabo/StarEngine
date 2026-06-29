#include "MeshComponent.h"

void MeshComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
}

void MeshComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

void MeshComponent::SetMeshPath(const std::string& path) {
    meshPath = path;
}