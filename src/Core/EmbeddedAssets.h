#pragma once
#include "StarPak.h"
#include "raylib.h"
#include <string>

class EmbeddedAssets {
public:
    static EmbeddedAssets& Get();

    void Init(const std::string& exePath);

    Image LoadImageAsset(const std::string& fileName);
    Texture2D LoadTextureAsset(const std::string& fileName);

private:
    EmbeddedAssets() = default;

    bool ReadBytes(const std::string& fileName, std::vector<uint8_t>& outBytes) const;
    std::string FindLooseAssetsDir() const;

    StarPak::Reader pak;
    bool pakLoaded = false;
    bool initialized = false;
};
