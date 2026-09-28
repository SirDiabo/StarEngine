#include "AssetCache.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/CookedFile.h"
#include "Core/ShadingModel.h"
#include "CollisionPrivate.h"
#include <cmath>

AssetCache& AssetCache::Get() {
    static AssetCache instance;
    return instance;
}

Model* AssetCache::GetOrLoadModel(const std::string& sourceMeshPath) {
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    auto it = meshCache.find(normalized);
    if (it != meshCache.end()) return &it->second;

    if (normalized.empty() || !FileExists(normalized.c_str())) return nullptr;

    Model model = LoadModel(normalized.c_str());
    if (model.meshCount == 0) {
        UnloadModel(model);
        return nullptr;
    }

    auto inserted = meshCache.emplace(normalized, model);
    return &inserted.first->second;
}

Model AssetCache::LoadPrivateModel(const std::string& sourceMeshPath) {
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    if (normalized.empty() || !FileExists(normalized.c_str())) return Model{};

    Model model = LoadModel(normalized.c_str());
    if (model.meshCount == 0) {
        UnloadModel(model);
        return Model{};
    }
    return model;
}

Texture2D* AssetCache::GetOrLoadTexture(const std::string& textureAssetPath) {
    auto it = textureCache.find(textureAssetPath);
    if (it != textureCache.end()) return &it->second;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(textureAssetPath, asset) || asset.assetType != "Texture") return nullptr;

    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;

    Texture2D tex = LoadTexture(resolved.c_str());
    if (tex.id == 0) return nullptr;

    auto inserted = textureCache.emplace(textureAssetPath, tex);
    return &inserted.first->second;
}

const StarMaterial* AssetCache::GetOrLoadMaterial(const std::string& materialAssetPath) {
    auto it = materialCache.find(materialAssetPath);
    if (it != materialCache.end()) return &it->second;

    StarMaterial material;
    if (!StarMaterial::LoadFromFile(materialAssetPath, material)) return nullptr;

    auto inserted = materialCache.emplace(materialAssetPath, material);
    return &inserted.first->second;
}

void AssetCache::ReloadMaterial(const std::string& materialAssetPath) {
    materialCache.erase(materialAssetPath);
}

void AssetCache::ReloadTexture(const std::string& textureAssetPath) {
    auto it = textureCache.find(textureAssetPath);
    if (it == textureCache.end()) return;
    UnloadTexture(it->second);
    textureCache.erase(it);
}

ModelAnimation* AssetCache::GetOrLoadAnimation(const std::string& animAssetPath) {
    auto it = animationCache.find(animAssetPath);
    if (it != animationCache.end()) return it->second;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(animAssetPath, asset) || asset.assetType != "Animation") return nullptr;

    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;

    auto srcIt = animationSourceCache.find(resolved);
    if (srcIt == animationSourceCache.end()) {
        LoadedAnimationSet set;
        set.animations = LoadModelAnimations(resolved.c_str(), &set.count);
        if (set.animations == nullptr || set.count <= 0) return nullptr;
        srcIt = animationSourceCache.emplace(resolved, set).first;
    }

    int idx = -1;
    for (int i = 0; i < srcIt->second.count; i++) {
        if (asset.animName == srcIt->second.animations[i].name) { idx = i; break; }
    }
    if (idx < 0 && asset.animSourceIndex >= 0 && asset.animSourceIndex < srcIt->second.count) {
        idx = asset.animSourceIndex;
    }
    if (idx < 0) return nullptr;

    ModelAnimation* anim = &srcIt->second.animations[idx];
    animationCache.emplace(animAssetPath, anim);
    return anim;
}

void AssetCache::ReloadAnimation(const std::string& animAssetPath) {
    StarAsset asset;
    std::string resolved;
    if (StarAsset::LoadFromFile(animAssetPath, asset)) {
        resolved = CookedFile::Open(asset.ResolveSourcePath());
        if (resolved.empty()) resolved = asset.ResolveSourcePath();
    }

    if (!resolved.empty()) {
        auto srcIt = animationSourceCache.find(resolved);
        if (srcIt != animationSourceCache.end()) {
            UnloadModelAnimations(srcIt->second.animations, srcIt->second.count);
            animationSourceCache.erase(srcIt);
        }
    }
    animationCache.clear();
}

BoundingBox AssetCache::GetOrComputeModelBounds(Model* model) {
    if (!model) return {};

    auto it = modelBoundsCache.find(model);
    if (it != modelBoundsCache.end()) return it->second;

    BoundingBox merged{};
    bool any = false;
    for (int i = 0; i < model->meshCount; i++) {
        BoundingBox meshBox = GetMeshBoundingBox(model->meshes[i]);
        if (!any) { merged = meshBox; any = true; continue; }
        merged.min.x = fminf(merged.min.x, meshBox.min.x);
        merged.min.y = fminf(merged.min.y, meshBox.min.y);
        merged.min.z = fminf(merged.min.z, meshBox.min.z);
        merged.max.x = fmaxf(merged.max.x, meshBox.max.x);
        merged.max.y = fmaxf(merged.max.y, meshBox.max.y);
        merged.max.z = fmaxf(merged.max.z, meshBox.max.z);
    }

    modelBoundsCache.emplace(model, merged);
    return merged;
}

std::shared_ptr<const CollisionPrivate::ModelBVH> AssetCache::GetOrBuildModelBVH(const std::string& sourceMeshPath) {
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    auto it = bvhCache.find(normalized);
    if (it != bvhCache.end()) return it->second;

    Model* model = GetOrLoadModel(sourceMeshPath);
    if (!model) return nullptr;

    std::shared_ptr<const CollisionPrivate::ModelBVH> bvh = CollisionPrivate::BuildModelBVH(*model);
    bvhCache.emplace(normalized, bvh);
    return bvh;
}

void AssetCache::ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths) {
    if (!model) return;

    for (int i = 0; i < model->materialCount; i++) {
        Color color = WHITE;
        bool lit = false;
        if ((size_t)i < materialPaths.size() && !materialPaths[i].empty()) {
            if (const StarMaterial* material = AssetCache::Get().GetOrLoadMaterial(materialPaths[i])) {
                color = material->baseColor;
                lit = material->lit;
                if (!material->textureAssetPath.empty()) {
                    if (Texture2D* tex = AssetCache::Get().GetOrLoadTexture(material->textureAssetPath)) {
                        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
                    }
                }
            }
        }
        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
        model->materials[i].shader = (lit && ShadingModel::ShouldUseLitShading())
            ? ShadingModel::GetLitShader() : ShadingModel::GetDefaultShader();
    }
}

void AssetCache::ReloadModel(const std::string& sourceMeshPath) {
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    bvhCache.erase(normalized);
    ++collisionGeneration;

    auto it = meshCache.find(normalized);
    if (it == meshCache.end()) return;
    modelBoundsCache.erase(&it->second);
    UnloadModel(it->second);
    meshCache.erase(it);
}

void AssetCache::Clear() {
    for (auto& kv : meshCache) UnloadModel(kv.second);
    modelBoundsCache.clear();
    bvhCache.clear();
    ++collisionGeneration;
    meshCache.clear();
    for (auto& kv : textureCache) UnloadTexture(kv.second);
    textureCache.clear();
    materialCache.clear();
    for (auto& kv : animationSourceCache) UnloadModelAnimations(kv.second.animations, kv.second.count);
    animationSourceCache.clear();
    animationCache.clear();
}
