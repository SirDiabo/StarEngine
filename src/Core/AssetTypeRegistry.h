#pragma once
#include <string>
#include <vector>
#include <functional>

struct AssetCreateVariant {
    std::string id;
    std::string label;
};

struct AssetTypeInfo {
    std::string typeName;
    std::string extension;
    std::string category;
    std::string moduleName;
    std::function<bool(const std::string& fullPath, const std::string& assetName)> createAsset;
    std::function<bool(const std::string& fullPath, const std::string& newAssetName)> renameAsset;
    std::function<std::vector<AssetCreateVariant>()> listCreateVariants;
    std::function<bool(const std::string& fullPath, const std::string& assetName, const std::string& variantId)> createAssetVariant;
};

class AssetTypeRegistry {
public:
    static AssetTypeRegistry& Get();

    void RegisterType(const AssetTypeInfo& info);
    std::vector<AssetTypeInfo> GetFilteredTypes() const;
    const std::vector<AssetTypeInfo>& GetAll() const { return types; }
    const AssetTypeInfo* FindByExtension(const std::string& extension) const;

private:
    std::vector<AssetTypeInfo> types;
};
