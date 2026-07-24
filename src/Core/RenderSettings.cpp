#include "RenderSettings.h"
#include "LayoutSerializer.h"
#include "SafeParse.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string ShadingTypeName(ShadingType type) {
    return type == ShadingType::ProjectedBlobShadows ? "ProjectedBlobShadows" : "Unlit";
}

ShadingType ShadingTypeFromName(const std::string& name) {
    return name == "ProjectedBlobShadows" ? ShadingType::ProjectedBlobShadows : ShadingType::Unlit;
}

RenderSettings& RenderSettings::Get() {
    static RenderSettings instance;
    return instance;
}

void RenderSettings::Load(const std::string& projectRoot) {
    shadingType = ShadingType::Unlit;
    renderScale = 1.0f;
    filePath = (fs::path(projectRoot) / "Saved" / "RenderSettings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    auto it = raw.find("shadingType");
    if (it != raw.end()) shadingType = ShadingTypeFromName(it->second);

    it = raw.find("renderScale");
    if (it != raw.end()) renderScale = SafeParse::ToFloat(it->second);
    if (renderScale <= 0.0f) renderScale = 1.0f;
}

void RenderSettings::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    raw["shadingType"] = ShadingTypeName(shadingType);
    raw["renderScale"] = std::to_string(renderScale);
    LayoutSerializer::Save(filePath, raw);
}

void RenderSettings::SetShadingType(ShadingType value) {
    shadingType = value;
    Save();
}

void RenderSettings::SetRenderScale(float value) {
    renderScale = value < 0.1f ? 0.1f : value;
    Save();
}