#pragma once
#include <string>

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

    const std::string& GetShadingType() const { return shadingType; }
    void SetShadingType(const std::string& value);

    LightingQuality GetLightingQuality() const { return lightingQuality; }
    void SetLightingQuality(LightingQuality value);

    float GetRenderScale() const { return renderScale; }
    void SetRenderScale(float value);

private:
    std::string filePath;
    std::string shadingType = "Unlit";
    LightingQuality lightingQuality = LightingQuality::PerVertex;
    float renderScale = 1.0f;
};
