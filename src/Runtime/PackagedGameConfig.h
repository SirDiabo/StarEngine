#pragma once
#include <string>

struct PackagedGameConfig {
    std::string name;
    int targetWidth = 640;
    int targetHeight = 480;
    std::string defaultMap;
    std::string renderer;
    std::string cipher;
    std::string cipherKey;

    static bool LoadFromFile(const std::string& path, PackagedGameConfig& out);
    static bool LoadFromText(const std::string& text, PackagedGameConfig& out);
    std::string ToText() const;
};
