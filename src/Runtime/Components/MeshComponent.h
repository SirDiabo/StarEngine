#pragma once
#include "raylib.h"
#include "SceneComponent.h"
#include <string>
#include <vector>

class MeshComponent : public SceneComponent {
public:
    std::string meshPath;
    std::vector<std::string> materialPaths; // parallel to the mesh asset's material slots
    bool castsShadow = true;

    const char* GetClassName() const override { return "MeshComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
    void SetMeshPath(const std::string& path);
    void SetMaterialPaths(const std::vector<std::string>& paths);
    void SetMaterialPath(int slotIndex, const std::string& path);

    Model* GetCachedModel();
    void InvalidateCachedModel() { cachedModel = nullptr; cachedModelPath.clear(); }

private:
    std::string cachedModelPath;
    Model* cachedModel = nullptr;
};

void RegisterMeshComponentNodes();