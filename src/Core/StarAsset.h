#pragma once
#include <string>

struct StarAsset {
    std::string assetType;
    std::string sourcePath;

    float importPosX = 0.0f, importPosY = 0.0f, importPosZ = 0.0f;
    float importRotX = 0.0f, importRotY = 0.0f, importRotZ = 0.0f;
    float importScaleX = 1.0f, importScaleY = 1.0f, importScaleZ = 1.0f;
    std::string defaultMaterialPath;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarAsset& outAsset);
};