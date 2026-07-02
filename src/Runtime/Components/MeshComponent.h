#pragma once
#include "raylib.h"
#include "SceneComponent.h"

class MeshComponent : public SceneComponent {
public:
    std::string meshPath;
    std::string materialPath;

    const char* GetClassName() const override { return "MeshComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
    void SetMeshPath(const std::string& path);
    void SetMaterialPath(const std::string& path);
};