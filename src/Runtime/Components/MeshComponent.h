#pragma once
#include "raylib.h"
#include "SceneComponent.h"

class MeshComponent : public SceneComponent {
public:
    Mesh mesh = {0};
    std::string meshPath;

    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
    void SetMeshPath(const std::string& path);
};