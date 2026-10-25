#include "StargraphAssetCache.h"
#include "Core/ContentPathResolver.h"

StargraphAssetCache& StargraphAssetCache::Get() {
    static StargraphAssetCache instance;
    return instance;
}

static std::string CanonicalKey(const std::string& classAssetPath) {
    std::string relative = ContentPathResolver::MakeRelative(classAssetPath);
    return relative.empty() ? classAssetPath : relative;
}

const Stargraph* StargraphAssetCache::GetOrLoad(const std::string& classAssetPath) {
    if (classAssetPath.empty()) return nullptr;
    std::string key = CanonicalKey(classAssetPath);

    auto it = cache.find(key);
    if (it != cache.end()) return &it->second;

    Stargraph graph;
    if (!graph.LoadFromFile(classAssetPath)) return nullptr;

    auto inserted = cache.emplace(key, std::move(graph));
    return &inserted.first->second;
}

void StargraphAssetCache::Invalidate(const std::string& classAssetPath) {
    cache.erase(CanonicalKey(classAssetPath));
}

void StargraphAssetCache::Clear() {
    cache.clear();
}
