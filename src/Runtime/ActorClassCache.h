#pragma once
#include "ActorAsset.h"
#include <memory>
#include <string>
#include <unordered_map>

class ActorClassCache {
public:
    static ActorClassCache& Get();

    const ActorAssetData* GetOrLoad(const std::string& assetPath);
    void Invalidate(const std::string& assetPath);
    void Clear();

private:
    std::unordered_map<std::string, std::unique_ptr<ActorAssetData>> cache;
};
