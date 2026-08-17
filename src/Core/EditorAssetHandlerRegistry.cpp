#include "EditorAssetHandlerRegistry.h"
#include "Module.h"

EditorAssetHandlerRegistry& EditorAssetHandlerRegistry::Get() {
    static EditorAssetHandlerRegistry instance;
    return instance;
}

void EditorAssetHandlerRegistry::RegisterHandler(const EditorAssetHandlerInfo& info) {
    for (EditorAssetHandlerInfo& existing : handlers) {
        if (existing.extension == info.extension) {
            existing = info;
            return;
        }
    }
    handlers.push_back(info);
}

const EditorAssetHandlerInfo* EditorAssetHandlerRegistry::FindHandler(const std::string& extension) const {
    for (const EditorAssetHandlerInfo& info : handlers) {
        if (info.extension != extension) continue;
        if (!info.moduleName.empty() && !ModuleManager::Get().IsModuleEnabled(info.moduleName)) continue;
        return &info;
    }
    return nullptr;
}
