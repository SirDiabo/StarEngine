#include "RenderSettings.h"
#include "LayoutSerializer.h"
#include "SafeParse.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string LightingQualityName(LightingQuality quality) {
    return quality == LightingQuality::PerPixel ? "PerPixel" : "PerVertex";
}

LightingQuality LightingQualityFromName(const std::string& name) {
    return name == "PerPixel" ? LightingQuality::PerPixel : LightingQuality::PerVertex;
}

RenderSettings& RenderSettings::Get() {
    static RenderSettings instance;
    return instance;
}

void RenderSettings::Load(const std::string& projectRoot) {
    shadingType = "Unlit";
    lightingQuality = LightingQuality::PerVertex;
    renderScale = 1.0f;
    filePath = (fs::path(projectRoot) / "Saved" / "RenderSettings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    auto it = raw.find("shadingType");
    if (it != raw.end() && !it->second.empty()) shadingType = it->second;

    it = raw.find("lightingQuality");
    if (it != raw.end()) lightingQuality = LightingQualityFromName(it->second);

    it = raw.find("renderScale");
    if (it != raw.end()) renderScale = SafeParse::ToFloat(it->second);
    if (renderScale <= 0.0f) renderScale = 1.0f;
}

void RenderSettings::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    raw["shadingType"] = shadingType;
    raw["lightingQuality"] = LightingQualityName(lightingQuality);
    raw["renderScale"] = std::to_string(renderScale);
    LayoutSerializer::Save(filePath, raw);
}

void RenderSettings::SetShadingType(const std::string& value) {
    shadingType = value.empty() ? "Unlit" : value;
    Save();
}

void RenderSettings::SetLightingQuality(LightingQuality value) {
    lightingQuality = value;
    Save();
}

void RenderSettings::SetRenderScale(float value) {
    renderScale = value < 0.1f ? 0.1f : value;
    Save();
}
