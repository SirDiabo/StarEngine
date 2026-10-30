#pragma once
#include <string>
#include <vector>

struct MaterialSlot {
    std::string usemtlName;
    std::string materialPath;
};

struct SkeletonBone {
    std::string name;
    int parentIndex = -1;
    float bindPosX = 0.0f, bindPosY = 0.0f, bindPosZ = 0.0f;
    float bindRotX = 0.0f, bindRotY = 0.0f, bindRotZ = 0.0f, bindRotW = 1.0f;
    float bindScaleX = 1.0f, bindScaleY = 1.0f, bindScaleZ = 1.0f;
};

struct StarAsset {
    std::string assetType;
    std::string sourcePath;

    std::string soundLoadMode = "Auto";
    std::string soundBus;

    std::string vendoredSourcePath;

    float importPosX = 0.0f, importPosY = 0.0f, importPosZ = 0.0f;
    float importRotX = 0.0f, importRotY = 0.0f, importRotZ = 0.0f;
    float importScaleX = 1.0f, importScaleY = 1.0f, importScaleZ = 1.0f;

    std::vector<MaterialSlot> materialSlots;

    bool hasSkeleton = false;
    std::vector<SkeletonBone> skeleton;

    int animSourceIndex = -1;
    std::string animName;
    int animFrameCount = 0;
    unsigned int animBoneCount = 0;
    float animFramesPerSecond = 30.0f;

    std::string ResolveSourcePath() const;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarAsset& outAsset);
};