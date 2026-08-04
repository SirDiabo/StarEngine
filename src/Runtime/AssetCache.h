#pragma once
#include "raylib.h"
#include "Core/StarMaterial.h"
#include <string>
#include <unordered_map>
#include <vector>

class AssetCache {
public:
    static AssetCache& Get();

    Model* GetOrLoadModel(const std::string& sourceMeshPath);
    Texture2D* GetOrLoadTexture(const std::string& textureAssetPath);
    const StarMaterial* GetOrLoadMaterial(const std::string& materialAssetPath);
    BoundingBox GetOrComputeModelBounds(Model* model);
    void ReloadModel(const std::string& sourceMeshPath);
    void ReloadMaterial(const std::string& materialAssetPath);
    void Clear();

    static void ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths);

private:
    std::unordered_map<std::string, Model> meshCache;
    std::unordered_map<std::string, Texture2D> textureCache;
    std::unordered_map<std::string, StarMaterial> materialCache;
    std::unordered_map<Model*, BoundingBox> modelBoundsCache;
};