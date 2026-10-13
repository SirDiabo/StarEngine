#pragma once
#include <istream>
#include <ostream>
#include <string>

class WidgetElement;
class Widget;
class IBehaviorContent;

struct WidgetAssetData {
    std::string assetName;
    std::string parentClass = "Widget";
    float uiScale = 1.0f;
    bool forceNativeResolution = false;
    std::string programmingInterface;
    WidgetElement* rootElement = nullptr;
    IBehaviorContent* behaviorContent = nullptr;
};

namespace WidgetAsset {
    bool SaveWidgetAsset(const std::string& path, const WidgetAssetData& data);
    bool LoadWidgetAsset(const std::string& path, WidgetAssetData& outData);
    bool WriteWidgetAsset(std::ostream& out, const WidgetAssetData& data, bool includeBehavior = true);
    bool ReadWidgetAsset(std::istream& in, WidgetAssetData& outData, bool loadBehavior = true);
    std::string SerializeWidgetAsset(const WidgetAssetData& data, bool includeBehavior = true);
    bool DeserializeWidgetAsset(const std::string& text, WidgetAssetData& outData, bool loadBehavior = true);
    Widget* InstantiateWidget(const std::string& path);
}
