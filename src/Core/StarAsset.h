#pragma once
#include <string>

struct StarAsset {
    std::string assetType;
    std::string sourcePath;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarAsset& outAsset);
};
