#include "StarMaterial.h"
#include "CookedFile.h"
#include <fstream>

bool StarMaterial::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarMaterial]\n";
    out << "ColorR=" << (int)baseColor.r << "\n";
    out << "ColorG=" << (int)baseColor.g << "\n";
    out << "ColorB=" << (int)baseColor.b << "\n";
    out << "ColorA=" << (int)baseColor.a << "\n";
    out << "Texture=" << textureAssetPath << "\n";

    return true;
}

bool StarMaterial::LoadFromFile(const std::string& path, StarMaterial& outMaterial) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '[') continue;

        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        if (key == "ColorR") outMaterial.baseColor.r = (unsigned char)atoi(value.c_str());
        else if (key == "ColorG") outMaterial.baseColor.g = (unsigned char)atoi(value.c_str());
        else if (key == "ColorB") outMaterial.baseColor.b = (unsigned char)atoi(value.c_str());
        else if (key == "ColorA") outMaterial.baseColor.a = (unsigned char)atoi(value.c_str());
        else if (key == "Texture") outMaterial.textureAssetPath = value;
    }

    return true;
}
