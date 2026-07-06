#include "AssetCache.h"
#include "Core/StarAsset.h"
#include "Core/CookedFile.h"

AssetCache& AssetCache::Get() {
    static AssetCache instance;
    return instance;
}

Model* AssetCache::GetOrLoadModel(const std::string& sourceMeshPath) {
    auto it = meshCache.find(sourceMeshPath);
    if (it != meshCache.end()) return &it->second;

    std::string resolved = CookedFile::Open(sourceMeshPath);
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;

    Model model = LoadModel(resolved.c_str());
    if (model.meshCount == 0) {
        UnloadModel(model);
        return nullptr;
    }

    auto inserted = meshCache.emplace(sourceMeshPath, model);
    return &inserted.first->second;
}

Texture2D* AssetCache::GetOrLoadTexture(const std::string& textureAssetPath) {
    auto it = textureCache.find(textureAssetPath);
    if (it != textureCache.end()) return &it->second;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(textureAssetPath, asset) || asset.assetType != "Texture") return nullptr;

    std::string resolved = CookedFile::Open(asset.sourcePath);
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;

    Texture2D tex = LoadTexture(resolved.c_str());
    if (tex.id == 0) return nullptr;

    auto inserted = textureCache.emplace(textureAssetPath, tex);
    return &inserted.first->second;
}

void AssetCache::Clear() {
    for (auto& kv : meshCache) UnloadModel(kv.second);
    meshCache.clear();
    for (auto& kv : textureCache) UnloadTexture(kv.second);
    textureCache.clear();
}
