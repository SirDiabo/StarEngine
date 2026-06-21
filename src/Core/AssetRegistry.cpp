#include "AssetRegistry.h"
#include "raylib.h"
#include <fstream>

AssetRegistry& AssetRegistry::Get() {
    static AssetRegistry instance;
    return instance;
}

AssetInfo* AssetRegistry::GetAsset(const std::string& virtualPath) {
    auto it = assets.find(virtualPath);
    return it != assets.end() ? &it->second : nullptr;
}

bool AssetRegistry::RegisterAsset(const std::string& virtualPath, const std::string& sourcePath, const std::string& cookedPath) {
    AssetInfo info;
    info.sourcePath = sourcePath;
    info.cookedPath = cookedPath;

    if (FileExists(sourcePath.c_str())) {
        info.lastModified = GetFileModTime(sourcePath.c_str());
        assets[virtualPath] = info;
        return true;
    }
    return false;
}

bool AssetRegistry::NeedsReimport(const std::string& virtualPath) {
    AssetInfo* info = GetAsset(virtualPath);
    if (!info) return true;
    if (!FileExists(info->sourcePath.c_str())) return true;
    return GetFileModTime(info->sourcePath.c_str()) > info->lastModified;
}

void AssetRegistry::Reimport(const std::string& virtualPath) {
    AssetInfo* info = GetAsset(virtualPath);
    if (info) {
        info->lastModified = GetFileModTime(info->sourcePath.c_str());
    }
}