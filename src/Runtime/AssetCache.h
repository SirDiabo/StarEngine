#pragma once
#include "raylib.h"
#include "Core/StarMaterial.h"
#include "Core/StarAsset.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace CollisionPrivate { struct ModelBVH; }

struct MeshSourceInfo {
    std::string assetType;
    std::string sourcePath;
    std::vector<MaterialSlot> materialSlots;
    long modTime = 0;
    double lastValidated = 0.0;
    bool valid = false;
};

class AssetCache {
public:
    static AssetCache& Get();

    const MeshSourceInfo* GetMeshSource(const std::string& meshAssetPath);
    void InvalidateMeshSource(const std::string& meshAssetPath);

    Model* GetOrLoadModel(const std::string& sourceMeshPath);
    Model LoadPrivateModel(const std::string& sourceMeshPath);
    Texture2D* GetOrLoadTexture(const std::string& textureAssetPath);
    const StarMaterial* GetOrLoadMaterial(const std::string& materialAssetPath);
    ModelAnimation* GetOrLoadAnimation(const std::string& animAssetPath);
    BoundingBox GetOrComputeModelBounds(Model* model);
    std::shared_ptr<const CollisionPrivate::ModelBVH> GetOrBuildModelBVH(const std::string& sourceMeshPath);
    unsigned GetCollisionGeneration() const { return collisionGeneration; }
    void ReloadModel(const std::string& sourceMeshPath);
    void ReloadTexture(const std::string& textureAssetPath);
    void ReloadMaterial(const std::string& materialAssetPath);
    void ReloadAnimation(const std::string& animAssetPath);
    void Clear();

    static void ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths);
    void CaptureDefaultMaterials(Model* model);
    static int SlotOffsetForSource(const std::string& sourceMeshPath);
    void RegisterModelInstance(Model* model, const std::string& sourceMeshPath);
    void ForgetModelInstance(Model* model);

private:
    struct LoadedAnimationSet {
        ModelAnimation* animations = nullptr;
        int count = 0;
    };

    std::unordered_map<std::string, Model> meshCache;
    std::unordered_map<std::string, Model*> meshRawLookup;
    std::unordered_map<std::string, MeshSourceInfo> meshSourceCache;
    std::unordered_map<std::string, Texture2D> textureCache;
    std::unordered_map<std::string, StarMaterial> materialCache;
    std::unordered_map<Model*, BoundingBox> modelBoundsCache;
    std::unordered_map<Model*, std::vector<Color>> modelDefaultColors;
    std::unordered_map<Model*, std::vector<Texture2D>> modelDefaultTextures;
    std::unordered_map<Model*, int> modelSlotOffset;
    std::unordered_map<std::string, std::shared_ptr<const CollisionPrivate::ModelBVH>> bvhCache;
    unsigned collisionGeneration = 0;
    std::unordered_map<std::string, LoadedAnimationSet> animationSourceCache;
    std::unordered_map<std::string, ModelAnimation*> animationCache;
};
