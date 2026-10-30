#include "StarAsset.h"
#include "CookedFile.h"
#include <cstdlib>
#include "Core/SafeParse.h"
#include <filesystem>
#include <fstream>

std::string StarAsset::ResolveSourcePath() const {
    if (!vendoredSourcePath.empty()) return vendoredSourcePath;
    if (!sourcePath.empty() && std::filesystem::exists(sourcePath)) return sourcePath;
    return sourcePath;
}

bool StarAsset::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarAsset]\n";
    out << "Type=" << assetType << "\n";
    out << "Source=" << sourcePath << "\n";
    out << "VendoredSource=" << vendoredSourcePath << "\n";
    if (assetType == "Sound") {
        out << "LoadMode=" << soundLoadMode << "\n";
        if (!soundBus.empty()) out << "Bus=" << soundBus << "\n";
    }
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
    out << "HasSkeleton=" << (hasSkeleton ? 1 : 0) << "\n";
    out << "SkeletonBoneCount=" << skeleton.size() << "\n";
    for (size_t i = 0; i < skeleton.size(); i++) {
        const SkeletonBone& b = skeleton[i];
        out << "SkeletonBone" << i << "Name=" << b.name << "\n";
        out << "SkeletonBone" << i << "Parent=" << b.parentIndex << "\n";
        out << "SkeletonBone" << i << "PosX=" << b.bindPosX << "\n";
        out << "SkeletonBone" << i << "PosY=" << b.bindPosY << "\n";
        out << "SkeletonBone" << i << "PosZ=" << b.bindPosZ << "\n";
        out << "SkeletonBone" << i << "RotX=" << b.bindRotX << "\n";
        out << "SkeletonBone" << i << "RotY=" << b.bindRotY << "\n";
        out << "SkeletonBone" << i << "RotZ=" << b.bindRotZ << "\n";
        out << "SkeletonBone" << i << "RotW=" << b.bindRotW << "\n";
        out << "SkeletonBone" << i << "ScaleX=" << b.bindScaleX << "\n";
        out << "SkeletonBone" << i << "ScaleY=" << b.bindScaleY << "\n";
        out << "SkeletonBone" << i << "ScaleZ=" << b.bindScaleZ << "\n";
    }
    out << "AnimSourceIndex=" << animSourceIndex << "\n";
    out << "AnimName=" << animName << "\n";
    out << "AnimFrameCount=" << animFrameCount << "\n";
    out << "AnimBoneCount=" << animBoneCount << "\n";
    out << "AnimFPS=" << animFramesPerSecond << "\n";

    return true;
}

bool StarAsset::LoadFromFile(const std::string& path, StarAsset& outAsset) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;

    int materialSlotCount = 0;
    int skeletonBoneCount = 0;

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
        else if (key == "LoadMode") outAsset.soundLoadMode = value;
        else if (key == "Bus") outAsset.soundBus = value;
        else if (key == "PosX") outAsset.importPosX = SafeParse::ToFloat(value);
        else if (key == "PosY") outAsset.importPosY = SafeParse::ToFloat(value);
        else if (key == "PosZ") outAsset.importPosZ = SafeParse::ToFloat(value);
        else if (key == "RotX") outAsset.importRotX = SafeParse::ToFloat(value);
        else if (key == "RotY") outAsset.importRotY = SafeParse::ToFloat(value);
        else if (key == "RotZ") outAsset.importRotZ = SafeParse::ToFloat(value);
        else if (key == "ScaleX") outAsset.importScaleX = SafeParse::ToFloat(value);
        else if (key == "ScaleY") outAsset.importScaleY = SafeParse::ToFloat(value);
        else if (key == "ScaleZ") outAsset.importScaleZ = SafeParse::ToFloat(value);
        else if (key == "MaterialSlotCount") materialSlotCount = SafeParse::ToInt(value);
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
        else if (key == "HasSkeleton") outAsset.hasSkeleton = SafeParse::ToInt(value) != 0;
        else if (key == "SkeletonBoneCount") skeletonBoneCount = SafeParse::ToInt(value);
        else if (key.rfind("SkeletonBone", 0) == 0) {
            size_t nameEnd = key.find_first_not_of("0123456789", 12);
            if (nameEnd == std::string::npos) continue;
            int index = std::atoi(key.substr(12, nameEnd - 12).c_str());
            std::string field = key.substr(nameEnd);
            if (index < 0) continue;
            if ((size_t)index >= outAsset.skeleton.size()) outAsset.skeleton.resize(index + 1);
            SkeletonBone& b = outAsset.skeleton[index];
            if (field == "Name") b.name = value;
            else if (field == "Parent") b.parentIndex = SafeParse::ToInt(value);
            else if (field == "PosX") b.bindPosX = SafeParse::ToFloat(value);
            else if (field == "PosY") b.bindPosY = SafeParse::ToFloat(value);
            else if (field == "PosZ") b.bindPosZ = SafeParse::ToFloat(value);
            else if (field == "RotX") b.bindRotX = SafeParse::ToFloat(value);
            else if (field == "RotY") b.bindRotY = SafeParse::ToFloat(value);
            else if (field == "RotZ") b.bindRotZ = SafeParse::ToFloat(value);
            else if (field == "RotW") b.bindRotW = SafeParse::ToFloat(value);
            else if (field == "ScaleX") b.bindScaleX = SafeParse::ToFloat(value);
            else if (field == "ScaleY") b.bindScaleY = SafeParse::ToFloat(value);
            else if (field == "ScaleZ") b.bindScaleZ = SafeParse::ToFloat(value);
        }
        else if (key == "AnimSourceIndex") outAsset.animSourceIndex = SafeParse::ToInt(value);
        else if (key == "AnimName") outAsset.animName = value;
        else if (key == "AnimFrameCount") outAsset.animFrameCount = SafeParse::ToInt(value);
        else if (key == "AnimBoneCount") outAsset.animBoneCount = (unsigned int)SafeParse::ToInt(value);
        else if (key == "AnimFPS") outAsset.animFramesPerSecond = SafeParse::ToFloat(value);
    }

    if (materialSlotCount > 0 && (size_t)materialSlotCount > outAsset.materialSlots.size()) {
        outAsset.materialSlots.resize(materialSlotCount);
    }
    if (skeletonBoneCount > 0 && (size_t)skeletonBoneCount > outAsset.skeleton.size()) {
        outAsset.skeleton.resize(skeletonBoneCount);
    }

    return true;
}
