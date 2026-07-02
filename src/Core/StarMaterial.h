#pragma once
#include "raylib.h"
#include <string>

// StarEngine materials are intentionally simple: a flat base color and an optional
// diffuse texture, applied unlit. There is no shader graph - this matches the
// fixed-function look of the target hardware generation.
struct StarMaterial {
    Color baseColor = WHITE;
    std::string textureAssetPath;

    bool SaveToFile(const std::string& path) const;
    static bool LoadFromFile(const std::string& path, StarMaterial& outMaterial);
};
