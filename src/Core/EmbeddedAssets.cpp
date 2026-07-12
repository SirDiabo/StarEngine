#include "EmbeddedAssets.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

EmbeddedAssets& EmbeddedAssets::Get() {
    static EmbeddedAssets instance;
    return instance;
}

void EmbeddedAssets::Init(const std::string& exePath) {
    if (initialized) return;
    initialized = true;
    if (!exePath.empty()) pakLoaded = pak.LoadFromExecutable(exePath);
}

std::string EmbeddedAssets::FindLooseAssetsDir() const {
    fs::path dir = GetApplicationDirectory();
    for (int i = 0; i < 5; i++) {
        fs::path candidate = dir / "Assets";
        if (fs::exists(candidate)) return candidate.string();
        candidate = dir / ".." / "Assets";
        if (fs::exists(candidate)) return candidate.string();
        dir = dir.parent_path();
    }
    return "";
}

bool EmbeddedAssets::ReadBytes(const std::string& fileName, std::vector<uint8_t>& outBytes) const {
    if (pakLoaded) {
        std::string virtualPath = "Assets/" + fileName;
        if (pak.HasEntry(virtualPath)) {
            outBytes = pak.ReadEntry(virtualPath);
            if (!outBytes.empty()) return true;
        }
    }

    std::string dir = FindLooseAssetsDir();
    if (dir.empty()) return false;

    fs::path p = fs::path(dir) / fileName;
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) return false;

    outBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !outBytes.empty();
}

Image EmbeddedAssets::LoadImageAsset(const std::string& fileName) {
    std::vector<uint8_t> bytes;
    if (!ReadBytes(fileName, bytes)) return Image{};

    fs::path ext = fs::path(fileName).extension();
    return ::LoadImageFromMemory(ext.string().c_str(), bytes.data(), (int)bytes.size());
}

Texture2D EmbeddedAssets::LoadTextureAsset(const std::string& fileName) {
    Image img = LoadImageAsset(fileName);
    if (!img.data) return Texture2D{};

    Texture2D tex = ::LoadTextureFromImage(img);
    ::UnloadImage(img);
    return tex;
}
