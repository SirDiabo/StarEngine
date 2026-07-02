#include "MeshComponent.h"

void MeshComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Static Mesh", &meshPath);
    outProperties.AddString("Material", &materialPath);
}

void MeshComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

void MeshComponent::SetMeshPath(const std::string& path) {
    meshPath = path;
}

void MeshComponent::SetMaterialPath(const std::string& path) {
    materialPath = path;
}