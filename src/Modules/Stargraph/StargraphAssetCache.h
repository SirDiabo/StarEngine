#pragma once
#include "Stargraph.h"
#include <string>
#include <unordered_map>

// One parse per session.
class StargraphAssetCache {
public:
    static StargraphAssetCache& Get();

    const Stargraph* GetOrLoad(const std::string& graphAssetPath);
    void Invalidate(const std::string& graphAssetPath);
    void Clear();

private:
    std::unordered_map<std::string, Stargraph> cache;
};