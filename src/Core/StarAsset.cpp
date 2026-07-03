#include "StarAsset.h"
#include <fstream>

bool StarAsset::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarAsset]\n";
    out << "Type=" << assetType << "\n";
    out << "Source=" << sourcePath << "\n";
    out << "PosX=" << importPosX << "\n";
    out << "PosY=" << importPosY << "\n";
    out << "PosZ=" << importPosZ << "\n";
    out << "RotX=" << importRotX << "\n";
    out << "RotY=" << importRotY << "\n";
    out << "RotZ=" << importRotZ << "\n";
    out << "ScaleX=" << importScaleX << "\n";
    out << "ScaleY=" << importScaleY << "\n";
    out << "ScaleZ=" << importScaleZ << "\n";
    out << "DefaultMaterial=" << defaultMaterialPath << "\n";

    return true;
}

bool StarAsset::LoadFromFile(const std::string& path, StarAsset& outAsset) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '[') continue;

        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        if (key == "Type") outAsset.assetType = value;
        else if (key == "Source") outAsset.sourcePath = value;
        else if (key == "PosX") outAsset.importPosX = std::stof(value);
        else if (key == "PosY") outAsset.importPosY = std::stof(value);
        else if (key == "PosZ") outAsset.importPosZ = std::stof(value);
        else if (key == "RotX") outAsset.importRotX = std::stof(value);
        else if (key == "RotY") outAsset.importRotY = std::stof(value);
        else if (key == "RotZ") outAsset.importRotZ = std::stof(value);
        else if (key == "ScaleX") outAsset.importScaleX = std::stof(value);
        else if (key == "ScaleY") outAsset.importScaleY = std::stof(value);
        else if (key == "ScaleZ") outAsset.importScaleZ = std::stof(value);
        else if (key == "DefaultMaterial") outAsset.defaultMaterialPath = value;
    }

    return true;
}