#include "AssetTypeRegistry.h"

AssetTypeRegistry& AssetTypeRegistry::Get() {
    static AssetTypeRegistry instance;
    return instance;
}

void AssetTypeRegistry::RegisterType(const AssetTypeInfo& info) {
    types.push_back(info);
}

const AssetTypeInfo* AssetTypeRegistry::FindByExtension(const std::string& extension) const {
    for (const AssetTypeInfo& info : types) {
        if (info.extension == extension) return &info;
    }
    return nullptr;
}
