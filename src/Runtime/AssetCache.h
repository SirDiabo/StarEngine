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
    Model LoadPrivateModel(const std::string& sourceMeshPath);
    Texture2D* GetOrLoadTexture(const std::string& textureAssetPath);
    const StarMaterial* GetOrLoadMaterial(const std::string& materialAssetPath);
    ModelAnimation* GetOrLoadAnimation(const std::string& animAssetPath);
    BoundingBox GetOrComputeModelBounds(Model* model);
    void ReloadModel(const std::string& sourceMeshPath);
    void ReloadTexture(const std::string& textureAssetPath);
    void ReloadMaterial(const std::string& materialAssetPath);
    void ReloadAnimation(const std::string& animAssetPath);
    void Clear();

    static void ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths);

private:
    struct LoadedAnimationSet {
        ModelAnimation* animations = nullptr;
        int count = 0;
    };

    std::unordered_map<std::string, Model> meshCache;
    std::unordered_map<std::string, Texture2D> textureCache;
    std::unordered_map<std::string, StarMaterial> materialCache;
    std::unordered_map<Model*, BoundingBox> modelBoundsCache;
    std::unordered_map<std::string, LoadedAnimationSet> animationSourceCache;
    std::unordered_map<std::string, ModelAnimation*> animationCache;
};
