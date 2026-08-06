#pragma once
#include <string>
#include <unordered_map>

struct AssetInfo {
    std::string sourcePath;
    std::string cookedPath;
    long lastModified = 0;
};

class AssetRegistry {
public:
    static AssetRegistry& Get();

    AssetInfo* GetAsset(const std::string& virtualPath);
    bool RegisterAsset(const std::string& virtualPath, const std::string& sourcePath, const std::string& cookedPath);
    bool NeedsReimport(const std::string& virtualPath);
    void Reimport(const std::string& virtualPath);
    void Clear() { assets.clear(); }

private:
    std::unordered_map<std::string, AssetInfo> assets;
};