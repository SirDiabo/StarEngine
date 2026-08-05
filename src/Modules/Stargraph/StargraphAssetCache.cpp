#include "StargraphAssetCache.h"
#include "Core/ContentPathResolver.h"

StargraphAssetCache& StargraphAssetCache::Get() {
    static StargraphAssetCache instance;
    return instance;
}

static std::string CanonicalKey(const std::string& graphAssetPath) {
    std::string relative = ContentPathResolver::MakeRelative(graphAssetPath);
    return relative.empty() ? graphAssetPath : relative;
}

const Stargraph* StargraphAssetCache::GetOrLoad(const std::string& graphAssetPath) {
    if (graphAssetPath.empty()) return nullptr;
    std::string key = CanonicalKey(graphAssetPath);

    auto it = cache.find(key);
    if (it != cache.end()) return &it->second;

    Stargraph graph;
    if (!graph.LoadFromFile(graphAssetPath)) return nullptr;

    auto inserted = cache.emplace(key, std::move(graph));
    return &inserted.first->second;
}

void StargraphAssetCache::Invalidate(const std::string& graphAssetPath) {
    cache.erase(CanonicalKey(graphAssetPath));
}

void StargraphAssetCache::Clear() {
    cache.clear();
}
