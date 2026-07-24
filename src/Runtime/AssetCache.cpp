#include "AssetCache.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "Core/CookedFile.h"

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

void AssetCache::ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths) {
    if (!model) return;

    for (int i = 0; i < model->materialCount; i++) {
        Color color = WHITE;
        if ((size_t)i < materialPaths.size() && !materialPaths[i].empty()) {
            StarMaterial material;
            if (StarMaterial::LoadFromFile(materialPaths[i], material)) {
                color = material.baseColor;
                if (!material.textureAssetPath.empty()) {
                    if (Texture2D* tex = AssetCache::Get().GetOrLoadTexture(material.textureAssetPath)) {
                        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
                    }
                }
            }
        }
        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
    }
}

void AssetCache::ReloadModel(const std::string& sourceMeshPath) {
    std::string normalized = CookedFile::Open(sourceMeshPath);
    if (normalized.empty()) normalized = sourceMeshPath;

    auto it = meshCache.find(normalized);
    if (it == meshCache.end()) return;
    UnloadModel(it->second);
    meshCache.erase(it);
}

void AssetCache::Clear() {
    for (auto& kv : meshCache) UnloadModel(kv.second);
    meshCache.clear();
    for (auto& kv : textureCache) UnloadTexture(kv.second);
    textureCache.clear();
}
