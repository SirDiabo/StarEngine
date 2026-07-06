#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>

class AssetCache {
public:
    static AssetCache& Get();

    Model* GetOrLoadModel(const std::string& sourceMeshPath);
    Texture2D* GetOrLoadTexture(const std::string& textureAssetPath);
    void Clear();

private:
    std::unordered_map<std::string, Model> meshCache;
    std::unordered_map<std::string, Texture2D> textureCache;
};
