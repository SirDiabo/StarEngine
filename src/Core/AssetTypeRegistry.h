#pragma once
#include <string>
#include <vector>
#include <functional>

struct AssetTypeInfo {
    std::string typeName;
    std::string extension;
    std::string category;
    std::function<bool(const std::string& fullPath, const std::string& assetName)> createAsset;
};

class AssetTypeRegistry {
public:
    static AssetTypeRegistry& Get();

    void RegisterType(const AssetTypeInfo& info);
    const std::vector<AssetTypeInfo>& GetAll() const { return types; }
    const AssetTypeInfo* FindByExtension(const std::string& extension) const;

private:
    std::vector<AssetTypeInfo> types;
};
