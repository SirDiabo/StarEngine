#pragma once
#include <string>
#include <vector>

struct MaterialSlot {
    std::string usemtlName;
    std::string materialPath;
};

struct StarAsset {
    std::string assetType;
    std::string sourcePath;

    std::string vendoredSourcePath;

    float importPosX = 0.0f, importPosY = 0.0f, importPosZ = 0.0f;
    float importRotX = 0.0f, importRotY = 0.0f, importRotZ = 0.0f;
    float importScaleX = 1.0f, importScaleY = 1.0f, importScaleZ = 1.0f;

    std::vector<MaterialSlot> materialSlots;

    std::string ResolveSourcePath() const;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarAsset& outAsset);
};