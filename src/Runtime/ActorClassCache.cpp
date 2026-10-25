#include "ActorClassCache.h"
#include "Core/ContentPathResolver.h"

ActorClassCache& ActorClassCache::Get() {
    static ActorClassCache instance;
    return instance;
}

static std::string CanonicalKey(const std::string& assetPath) {
    std::string relative = ContentPathResolver::MakeRelative(assetPath);
    return relative.empty() ? assetPath : relative;
}

const ActorAssetData* ActorClassCache::GetOrLoad(const std::string& assetPath) {
    if (assetPath.empty()) return nullptr;
    std::string key = CanonicalKey(assetPath);

    auto it = cache.find(key);
    if (it != cache.end()) return it->second.get();

    auto data = std::make_unique<ActorAssetData>();
    if (!ActorAsset::Load(assetPath, *data)) return nullptr;

    const ActorAssetData* result = data.get();
    cache.emplace(key, std::move(data));
    return result;
}

void ActorClassCache::Invalidate(const std::string& assetPath) {
    cache.erase(CanonicalKey(assetPath));
}

void ActorClassCache::Clear() {
    cache.clear();
}
