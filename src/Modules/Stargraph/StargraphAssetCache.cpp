#include "StargraphAssetCache.h"

StargraphAssetCache& StargraphAssetCache::Get() {
    static StargraphAssetCache instance;
    return instance;
}

const Stargraph* StargraphAssetCache::GetOrLoad(const std::string& graphAssetPath) {
    if (graphAssetPath.empty()) return nullptr;

    auto it = cache.find(graphAssetPath);
    if (it != cache.end()) return &it->second;

    Stargraph graph;
    if (!graph.LoadFromFile(graphAssetPath)) return nullptr;

    auto inserted = cache.emplace(graphAssetPath, std::move(graph));
    return &inserted.first->second;
}

void StargraphAssetCache::Invalidate(const std::string& graphAssetPath) {
    cache.erase(graphAssetPath);
}

void StargraphAssetCache::Clear() {
    cache.clear();
}
