#include "ShadowMapSettings.h"
#include "Core/LayoutSerializer.h"
#include "Core/SafeParse.h"
#include <filesystem>

namespace fs = std::filesystem;

ShadowMapSettings& ShadowMapSettings::Get() {
    static ShadowMapSettings instance;
    return instance;
}

void ShadowMapSettings::Load(const std::string& projectRoot) {
    shadowMapSize = 2048;
    sunCoverageHalfSize = 4000.0f;
    shadowStrength = 0.75f;
    filePath = (fs::path(projectRoot) / "Saved" / "ShadowMapSettings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    auto it = raw.find("shadowMapSize");
    if (it != raw.end()) shadowMapSize = SafeParse::ToInt(it->second, 2048);
    if (shadowMapSize != 1024 && shadowMapSize != 2048 && shadowMapSize != 4096) shadowMapSize = 2048;

    it = raw.find("sunCoverageHalfSize");
    if (it != raw.end()) sunCoverageHalfSize = SafeParse::ToFloat(it->second, 4000.0f);
    if (sunCoverageHalfSize < 100.0f) sunCoverageHalfSize = 100.0f;

    it = raw.find("shadowStrength");
    if (it != raw.end()) shadowStrength = SafeParse::ToFloat(it->second, 0.75f);
    if (shadowStrength < 0.0f) shadowStrength = 0.0f;
    if (shadowStrength > 1.0f) shadowStrength = 1.0f;
}

void ShadowMapSettings::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    raw["shadowMapSize"] = std::to_string(shadowMapSize);
    raw["sunCoverageHalfSize"] = std::to_string(sunCoverageHalfSize);
    raw["shadowStrength"] = std::to_string(shadowStrength);
    LayoutSerializer::Save(filePath, raw);
}

void ShadowMapSettings::SetShadowMapSize(int value) {
    if (value != 1024 && value != 2048 && value != 4096) return;
    shadowMapSize = value;
    Save();
}

void ShadowMapSettings::SetSunCoverageHalfSize(float value) {
    sunCoverageHalfSize = value < 100.0f ? 100.0f : value;
    Save();
}

void ShadowMapSettings::SetShadowStrength(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    shadowStrength = value;
    Save();
}
