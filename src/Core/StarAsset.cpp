#include "StarAsset.h"
#include "CookedFile.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

std::string StarAsset::ResolveSourcePath() const {
    if (!sourcePath.empty() && std::filesystem::exists(sourcePath)) return sourcePath;
    if (!vendoredSourcePath.empty()) return vendoredSourcePath;
    return sourcePath;
}

bool StarAsset::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarAsset]\n";
    out << "Type=" << assetType << "\n";
    out << "Source=" << sourcePath << "\n";
    out << "VendoredSource=" << vendoredSourcePath << "\n";
    out << "PosX=" << importPosX << "\n";
    out << "PosY=" << importPosY << "\n";
    out << "PosZ=" << importPosZ << "\n";
    out << "RotX=" << importRotX << "\n";
    out << "RotY=" << importRotY << "\n";
    out << "RotZ=" << importRotZ << "\n";
    out << "ScaleX=" << importScaleX << "\n";
    out << "ScaleY=" << importScaleY << "\n";
    out << "ScaleZ=" << importScaleZ << "\n";
    out << "MaterialSlotCount=" << materialSlots.size() << "\n";
    for (size_t i = 0; i < materialSlots.size(); i++) {
        out << "MaterialSlot" << i << "Name=" << materialSlots[i].usemtlName << "\n";
        out << "MaterialSlot" << i << "Path=" << materialSlots[i].materialPath << "\n";
    }

    return true;
}

bool StarAsset::LoadFromFile(const std::string& path, StarAsset& outAsset) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;

    int materialSlotCount = 0;

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
        else if (key == "VendoredSource") outAsset.vendoredSourcePath = value;
        else if (key == "PosX") outAsset.importPosX = std::stof(value);
        else if (key == "PosY") outAsset.importPosY = std::stof(value);
        else if (key == "PosZ") outAsset.importPosZ = std::stof(value);
        else if (key == "RotX") outAsset.importRotX = std::stof(value);
        else if (key == "RotY") outAsset.importRotY = std::stof(value);
        else if (key == "RotZ") outAsset.importRotZ = std::stof(value);
        else if (key == "ScaleX") outAsset.importScaleX = std::stof(value);
        else if (key == "ScaleY") outAsset.importScaleY = std::stof(value);
        else if (key == "ScaleZ") outAsset.importScaleZ = std::stof(value);
        else if (key == "MaterialSlotCount") materialSlotCount = std::atoi(value.c_str());
        else if (key.rfind("MaterialSlot", 0) == 0) {
            size_t nameEnd = key.find_first_not_of("0123456789", 12);
            if (nameEnd == std::string::npos) continue;
            int index = std::atoi(key.substr(12, nameEnd - 12).c_str());
            std::string field = key.substr(nameEnd);
            if (index < 0) continue;
            if ((size_t)index >= outAsset.materialSlots.size()) outAsset.materialSlots.resize(index + 1);
            if (field == "Name") outAsset.materialSlots[index].usemtlName = value;
            else if (field == "Path") outAsset.materialSlots[index].materialPath = value;
        }
    }

    if (materialSlotCount > 0 && (size_t)materialSlotCount > outAsset.materialSlots.size()) {
        outAsset.materialSlots.resize(materialSlotCount);
    }

    return true;
}
