#pragma once
#include <string>
#include <unordered_map>

struct PackagedGameConfig {
    std::string name;
    float renderScale = 1.0f;
    std::string shadingType;
    std::string lightingQuality;
    std::string defaultMap;
    std::string renderer;
    std::string cipher;
    std::string cipherKey;
    std::unordered_map<std::string, std::string> moduleSettings;

    static bool LoadFromFile(const std::string& path, PackagedGameConfig& out);
    static bool LoadFromText(const std::string& text, PackagedGameConfig& out);
    std::string ToText() const;
};