#pragma once
#include "WidgetAsset.h"
#include <string>
#include <unordered_map>

class WidgetAssetCache {
public:
    static WidgetAssetCache& Get();

    const WidgetAssetData* GetOrLoad(const std::string& widgetAssetPath);
    void Invalidate(const std::string& widgetAssetPath);
    void Clear();

    ~WidgetAssetCache();

private:
    std::unordered_map<std::string, WidgetAssetData> cache;
};
