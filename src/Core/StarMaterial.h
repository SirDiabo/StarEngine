#pragma once
#include "raylib.h"
#include <string>

struct StarMaterial {
    Color baseColor = WHITE;
    std::string textureAssetPath;
    
    bool lit = false;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarMaterial& outMaterial);
};