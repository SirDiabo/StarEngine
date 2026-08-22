#pragma once
#include <string>
#include <unordered_map>

enum class ShadowAAMethod {
    None,
    Low,
    High,
};

std::string ShadowAAMethodName(ShadowAAMethod method);
ShadowAAMethod ShadowAAMethodFromName(const std::string& name);

class ShadowMapSettings {
public:
    static ShadowMapSettings& Get();

    void Load(const std::string& projectRoot);
    void Save() const;

    std::unordered_map<std::string, std::string> ToKeyValues() const;
    void ApplyKeyValues(const std::unordered_map<std::string, std::string>& values);

    int GetShadowMapSize() const { return shadowMapSize; }
    void SetShadowMapSize(int value);

    float GetSunCoverageHalfSize() const { return sunCoverageHalfSize; }
    void SetSunCoverageHalfSize(float value);

    float GetShadowStrength() const { return shadowStrength; }
    void SetShadowStrength(float value);

    ShadowAAMethod GetShadowAAMethod() const { return shadowAAMethod; }
    void SetShadowAAMethod(ShadowAAMethod value);

private:
    std::string filePath;
    int shadowMapSize = 2048;
    float sunCoverageHalfSize = 4000.0f;
    float shadowStrength = 0.75f;
    ShadowAAMethod shadowAAMethod = ShadowAAMethod::Low;
};
