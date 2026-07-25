#pragma once
#include <string>

enum class ShadingType {
    Unlit,
    ProjectedBlobShadows,
};

std::string ShadingTypeName(ShadingType type);
ShadingType ShadingTypeFromName(const std::string& name);

enum class LightingQuality {
    PerVertex,
    PerPixel,
};

std::string LightingQualityName(LightingQuality quality);
LightingQuality LightingQualityFromName(const std::string& name);

class RenderSettings {
public:
    static RenderSettings& Get();

    void Load(const std::string& projectRoot);
    void Save() const;

    ShadingType GetShadingType() const { return shadingType; }
    void SetShadingType(ShadingType value);

    LightingQuality GetLightingQuality() const { return lightingQuality; }
    void SetLightingQuality(LightingQuality value);

    float GetRenderScale() const { return renderScale; }
    void SetRenderScale(float value);

private:
    std::string filePath;
    ShadingType shadingType = ShadingType::Unlit;
    LightingQuality lightingQuality = LightingQuality::PerVertex;
    float renderScale = 1.0f;
};