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

bool EmbeddedAssets::ReadModuleBytes(const std::string& moduleName, const std::string& fileName, std::vector<uint8_t>& outBytes) const {
    if (moduleName.empty() || fileName.empty()) return false;

    if (pakLoaded) {
        std::string virtualPath = "Assets/Modules/" + moduleName + "/" + fileName;
        if (pak.HasEntry(virtualPath)) {
            outBytes = pak.ReadEntry(virtualPath);
            if (!outBytes.empty()) return true;
        }
    }

#ifdef STARENGINE_SOURCE_ROOT
    fs::path p = fs::path(STARENGINE_SOURCE_ROOT) / "Modules" / moduleName / "Assets" / fileName;
    std::ifstream in(p, std::ios::binary);
    if (in.is_open()) {
        outBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return !outBytes.empty();
    }
#endif
    return false;
}

Texture2D EmbeddedAssets::TextureFromBytes(const std::string& fileName, const std::vector<uint8_t>& bytes) {
    fs::path ext = fs::path(fileName).extension();
    Image img = ::LoadImageFromMemory(ext.string().c_str(), bytes.data(), (int)bytes.size());
    if (!img.data) return Texture2D{};

    Texture2D tex = ::LoadTextureFromImage(img);
    ::UnloadImage(img);
    if (tex.id != 0) {
        ::GenTextureMipmaps(&tex);
        ::SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    }
    return tex;
}

Texture2D EmbeddedAssets::LoadModuleTextureAsset(const std::string& moduleName, const std::string& fileName) {
    std::vector<uint8_t> bytes;
    if (!ReadModuleBytes(moduleName, fileName, bytes)) return Texture2D{};
    return TextureFromBytes(fileName, bytes);
}

Image EmbeddedAssets::LoadImageAsset(const std::string& fileName) {
    std::vector<uint8_t> bytes;
    if (!ReadBytes(fileName, bytes)) return Image{};

    fs::path ext = fs::path(fileName).extension();
    return ::LoadImageFromMemory(ext.string().c_str(), bytes.data(), (int)bytes.size());
}

Texture2D EmbeddedAssets::LoadTextureAsset(const std::string& fileName) {
    std::vector<uint8_t> bytes;
    if (!ReadBytes(fileName, bytes)) return Texture2D{};
    return TextureFromBytes(fileName, bytes);
}
