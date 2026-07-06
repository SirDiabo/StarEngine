#include "PackagedGameConfig.h"
#include <fstream>

bool PackagedGameConfig::LoadFromFile(const std::string& path, PackagedGameConfig& out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        if (key == "name") out.name = value;
        else if (key == "targetWidth") out.targetWidth = std::stoi(value);
        else if (key == "targetHeight") out.targetHeight = std::stoi(value);
        else if (key == "defaultMap") out.defaultMap = value;
        else if (key == "renderer") out.renderer = value;
        else if (key == "cipher") out.cipher = value;
        else if (key == "cipherKey") out.cipherKey = value;
    }
    return true;
}
