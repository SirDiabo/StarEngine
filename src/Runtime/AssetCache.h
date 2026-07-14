#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

class AssetCache {
public:
    static AssetCache& Get();

    Model* GetOrLoadModel(const std::string& sourceMeshPath);
    Texture2D* GetOrLoadTexture(const std::string& textureAssetPath);
    void Clear();

    static void ApplyMaterialSlots(Model* model, const std::vector<std::string>& materialPaths);

private:
    std::unordered_map<std::string, Model> meshCache;
    std::unordered_map<std::string, Texture2D> textureCache;
};
