#pragma once
#include "Stargraph.h"
#include <string>

class WidgetElement;
class Widget;

struct WidgetAssetData {
    std::string assetName;
    std::string parentClass = "Widget";
    float uiScale = 1.0f;
    bool forceNativeResolution = false;
    WidgetElement* rootElement = nullptr;
    Stargraph graph;
};

namespace WidgetAsset {
    bool SaveWidgetAsset(const std::string& path, const WidgetAssetData& data);
    bool LoadWidgetAsset(const std::string& path, WidgetAssetData& outData);
    Widget* InstantiateWidget(const std::string& path);
}
