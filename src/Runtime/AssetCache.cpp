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

const MeshSourceInfo* AssetCache::GetMeshSource(const std::string& meshAssetPath) {
    constexpr double kRevalidateSeconds = 0.5;
    double now = GetTime();

    auto it = meshSourceCache.find(meshAssetPath);
    if (it != meshSourceCache.end()) {
        MeshSourceInfo& info = it->second;
        if (now - info.lastValidated < kRevalidateSeconds) return info.valid ? &info : nullptr;
        info.lastValidated = now;
        std::string opened = CookedFile::Open(meshAssetPath);
        long modTime = opened.empty() ? 0 : GetFileModTime(opened.c_str());
        if (modTime == info.modTime && (info.valid || modTime == 0)) return info.valid ? &info : nullptr;
    }

    MeshSourceInfo& info = meshSourceCache[meshAssetPath];
    info = MeshSourceInfo{};
    info.lastValidated = now;

    std::string opened = CookedFile::Open(meshAssetPath);
    info.modTime = opened.empty() ? 0 : GetFileModTime(opened.c_str());

    StarAsset asset;
    if (!StarAsset::LoadFromFile(meshAssetPath, asset)) return nullptr;
    info.assetType = asset.assetType;
    info.sourcePath = asset.ResolveSourcePath();
    info.materialSlots = asset.materialSlots;
    info.valid = true;
    return &info;
}

void AssetCache::InvalidateMeshSource(const std::string& meshAssetPath) {
    meshSourceCache.erase(meshAssetPath);
}

Model* AssetCache::GetOrLoadModel(const std::string& sourceMeshPath) {
    auto rawHit = meshRawLookup.find(sourceMeshPath);
    if (rawHit != meshRawLookup.end()) return rawHit->second;

    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    auto it = meshCache.find(normalized);
    if (it != meshCache.end()) {
        meshRawLookup[sourceMeshPath] = &it->second;
        return &it->second;
    }

    if (normalized.empty() || !FileExists(normalized.c_str())) return nullptr;

    Model model = LoadModel(normalized.c_str());
    if (model.meshCount == 0) {
        UnloadModel(model);
        return nullptr;
    }

    auto inserted = meshCache.emplace(normalized, model);
    Model* cached = &inserted.first->second;
    modelSlotOffset[cached] = SlotOffsetForSource(normalized);
    CaptureDefaultMaterials(cached);
    meshRawLookup[sourceMeshPath] = cached;
    return cached;
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

int AssetCache::SlotOffsetForSource(const std::string& sourceMeshPath) {
    std::string ext = GetFileExtension(sourceMeshPath.c_str());
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    return (ext == ".gltf" || ext == ".glb") ? 1 : 0;
}

void AssetCache::RegisterModelInstance(Model* model, const std::string& sourceMeshPath) {
    if (!model) return;
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;
    modelDefaultColors.erase(model);
    modelDefaultTextures.erase(model);
    modelSlotOffset[model] = SlotOffsetForSource(normalized);
}

void AssetCache::ForgetModelInstance(Model* model) {
    modelDefaultColors.erase(model);
    modelDefaultTextures.erase(model);
    modelSlotOffset.erase(model);
}

void AssetCache::CaptureDefaultMaterials(Model* model) {
    if (!model || modelDefaultColors.count(model)) return;
    std::vector<Color> colors;
    std::vector<Texture2D> textures;
    colors.reserve(model->materialCount);
    textures.reserve(model->materialCount);
    for (int i = 0; i < model->materialCount; i++) {
        colors.push_back(model->materials[i].maps[MATERIAL_MAP_DIFFUSE].color);
        textures.push_back(model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture);
    }
    modelDefaultColors.emplace(model, std::move(colors));
    modelDefaultTextures.emplace(model, std::move(textures));
}

void AssetCache::ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths) {
    if (!model) return;
    AssetCache::Get().CaptureDefaultMaterials(model);

    auto defaultColors = AssetCache::Get().modelDefaultColors.find(model);
    auto defaultTextures = AssetCache::Get().modelDefaultTextures.find(model);
    bool hasDefaults = defaultColors != AssetCache::Get().modelDefaultColors.end() &&
                        defaultTextures != AssetCache::Get().modelDefaultTextures.end();

    auto offsetIt = AssetCache::Get().modelSlotOffset.find(model);
    int slotOffset = offsetIt != AssetCache::Get().modelSlotOffset.end() ? offsetIt->second : 0;

    for (int i = 0; i < model->materialCount; i++) {
        int slotIndex = i - slotOffset;
        Color color = hasDefaults && (size_t)i < defaultColors->second.size() ? defaultColors->second[i] : WHITE;
        Texture2D texture = hasDefaults && (size_t)i < defaultTextures->second.size() ? defaultTextures->second[i] : Texture2D{0};
        bool lit = false;
        if (slotIndex >= 0 && (size_t)slotIndex < materialPaths.size() && !materialPaths[slotIndex].empty()) {
            if (const StarMaterial* material = AssetCache::Get().GetOrLoadMaterial(materialPaths[slotIndex])) {
                color = material->baseColor;
                lit = material->lit;
                if (!material->textureAssetPath.empty()) {
                    if (Texture2D* tex = AssetCache::Get().GetOrLoadTexture(material->textureAssetPath)) {
                        texture = *tex;
                    }
                }
            }
        }
        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
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
    for (auto raw = meshRawLookup.begin(); raw != meshRawLookup.end();) {
        if (raw->second == &it->second) raw = meshRawLookup.erase(raw);
        else ++raw;
    }
    modelBoundsCache.erase(&it->second);
    modelDefaultColors.erase(&it->second);
    modelDefaultTextures.erase(&it->second);
    modelSlotOffset.erase(&it->second);
    UnloadModel(it->second);
    meshCache.erase(it);
}

void AssetCache::Clear() {
    for (auto& kv : meshCache) UnloadModel(kv.second);
    meshRawLookup.clear();
    meshSourceCache.clear();
    modelBoundsCache.clear();
    modelDefaultColors.clear();
    modelDefaultTextures.clear();
    modelSlotOffset.clear();
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
