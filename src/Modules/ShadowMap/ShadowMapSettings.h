#pragma once
#include <string>

class ShadowMapSettings {
public:
    static ShadowMapSettings& Get();

    void Load(const std::string& projectRoot);
    void Save() const;

    int GetShadowMapSize() const { return shadowMapSize; }
    void SetShadowMapSize(int value);

    float GetSunCoverageHalfSize() const { return sunCoverageHalfSize; }
    void SetSunCoverageHalfSize(float value);

    float GetShadowStrength() const { return shadowStrength; }
    void SetShadowStrength(float value);

private:
    std::string filePath;
    int shadowMapSize = 2048;
    float sunCoverageHalfSize = 4000.0f;
    float shadowStrength = 0.75f;
};
