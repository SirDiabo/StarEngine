#include "ShadowMapSettings.h"
#include "Core/LayoutSerializer.h"
#include "Core/SafeParse.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string ShadowAAMethodName(ShadowAAMethod method) {
    switch (method) {
        case ShadowAAMethod::None: return "None";
        case ShadowAAMethod::High: return "High";
        case ShadowAAMethod::Low:
        default: return "Low";
    }
}

ShadowAAMethod ShadowAAMethodFromName(const std::string& name) {
    if (name == "None") return ShadowAAMethod::None;
    if (name == "High") return ShadowAAMethod::High;
    return ShadowAAMethod::Low;
}

ShadowMapSettings& ShadowMapSettings::Get() {
    static ShadowMapSettings instance;
    return instance;
}

std::unordered_map<std::string, std::string> ShadowMapSettings::ToKeyValues() const {
    std::unordered_map<std::string, std::string> raw;
    raw["shadowMapSize"] = std::to_string(shadowMapSize);
    raw["sunCoverageHalfSize"] = std::to_string(sunCoverageHalfSize);
    raw["shadowStrength"] = std::to_string(shadowStrength);
    raw["shadowAAMethod"] = ShadowAAMethodName(shadowAAMethod);
    return raw;
}

void ShadowMapSettings::ApplyKeyValues(const std::unordered_map<std::string, std::string>& raw) {
    shadowMapSize = 2048;
    sunCoverageHalfSize = 4000.0f;
    shadowStrength = 0.75f;
    shadowAAMethod = ShadowAAMethod::Low;

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

    it = raw.find("shadowAAMethod");
    if (it != raw.end()) shadowAAMethod = ShadowAAMethodFromName(it->second);
}

void ShadowMapSettings::Load(const std::string& projectRoot) {
    filePath = (fs::path(projectRoot) / "Saved" / "ShadowMapSettings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) {
        ApplyKeyValues({});
        return;
    }
    ApplyKeyValues(raw);
}

void ShadowMapSettings::Save() const {
    if (filePath.empty()) return;
    LayoutSerializer::Save(filePath, ToKeyValues());
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

void ShadowMapSettings::SetShadowAAMethod(ShadowAAMethod value) {
    shadowAAMethod = value;
    Save();
}
