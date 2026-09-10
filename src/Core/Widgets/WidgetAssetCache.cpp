#include "WidgetAssetCache.h"
#include "Core/ContentPathResolver.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "WidgetElement.h"

WidgetAssetCache& WidgetAssetCache::Get() {
    static WidgetAssetCache instance;
    return instance;
}

static std::string CanonicalKey(const std::string& widgetAssetPath) {
    std::string relative = ContentPathResolver::MakeRelative(widgetAssetPath);
    return relative.empty() ? widgetAssetPath : relative;
}

const WidgetAssetData* WidgetAssetCache::GetOrLoad(const std::string& widgetAssetPath) {
    if (widgetAssetPath.empty()) return nullptr;
    std::string key = CanonicalKey(widgetAssetPath);

    auto it = cache.find(key);
    if (it != cache.end()) return &it->second;

    WidgetAssetData data;
    if (!WidgetAsset::LoadWidgetAsset(widgetAssetPath, data)) {
        delete data.rootElement;
        delete data.behaviorContent;
        return nullptr;
    }

    auto inserted = cache.emplace(key, std::move(data));
    return &inserted.first->second;
}

void WidgetAssetCache::Invalidate(const std::string& widgetAssetPath) {
    auto it = cache.find(CanonicalKey(widgetAssetPath));
    if (it == cache.end()) return;
    delete it->second.rootElement;
    delete it->second.behaviorContent;
    cache.erase(it);
}

void WidgetAssetCache::Clear() {
    for (auto& kv : cache) {
        delete kv.second.rootElement;
        delete kv.second.behaviorContent;
    }
    cache.clear();
}

WidgetAssetCache::~WidgetAssetCache() {
    Clear();
}
