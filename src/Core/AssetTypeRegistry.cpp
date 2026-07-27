#include "AssetTypeRegistry.h"
#include "Module.h"

AssetTypeRegistry& AssetTypeRegistry::Get() {
    static AssetTypeRegistry instance;
    return instance;
}

void AssetTypeRegistry::RegisterType(const AssetTypeInfo& info) {
    for (AssetTypeInfo& existing : types) {
        if (existing.extension == info.extension) {
            existing = info;
            return;
        }
    }
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