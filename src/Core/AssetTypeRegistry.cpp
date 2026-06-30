#include "AssetTypeRegistry.h"
#include "Module.h"

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

std::vector<AssetTypeInfo> AssetTypeRegistry::GetFilteredTypes() const {
    std::vector<AssetTypeInfo> result;
    for (const AssetTypeInfo& info : types) {
        if (info.moduleName.empty()) {
            result.push_back(info);
            continue;
        }
        if (ModuleManager::Get().IsModuleEnabled(info.moduleName)) {
            result.push_back(info);
        }
    }
    return result;
}
