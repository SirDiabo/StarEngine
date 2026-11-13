#include "PathSafety.h"
#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace fs = std::filesystem;

namespace {

std::string LowerExtension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

bool ExtensionIn(const fs::path& path, std::initializer_list<const char*> allowed) {
    const std::string ext = LowerExtension(path);
    for (const char* candidate : allowed) {
        if (ext == candidate) return true;
    }
    return false;
}

fs::path ResolveReference(const fs::path& baseDir, const std::string& reference, bool (*extensionOk)(const fs::path&)) {
    if (reference.empty() || reference.size() > 1024 || reference.find('\0') != std::string::npos) return {};

    std::string normalized = reference;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const fs::path relative(normalized);

    std::error_code ec;
    auto usable = [&](const fs::path& candidate) {
        return extensionOk(candidate) && fs::is_regular_file(candidate, ec) && PathSafety::IsInside(baseDir, candidate);
    };

    if (!relative.is_absolute() && !relative.has_root_name() && !relative.has_root_directory()) {
        const fs::path joined = (baseDir / relative).lexically_normal();
        if (usable(joined)) return joined;
    }

    const std::string leaf = relative.filename().string();
    if (PathSafety::IsPlainFileName(leaf)) {
        const fs::path candidate = baseDir / leaf;
        if (usable(candidate)) return candidate;
    }
    return {};
}

}

namespace PathSafety {

bool IsPlainFileName(const std::string& name) {
    if (name.empty() || name.size() > 255) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || (unsigned char)c < 32) return false;
    }
    return true;
}

bool IsInside(const fs::path& root, const fs::path& candidate) {
    std::error_code ec;
    const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
    if (ec) return false;
    const fs::path canonicalCandidate = fs::weakly_canonical(candidate, ec);
    if (ec) return false;

    auto rootIt = canonicalRoot.begin();
    auto rootEnd = canonicalRoot.end();
    auto candIt = canonicalCandidate.begin();
    auto candEnd = canonicalCandidate.end();
    for (; rootIt != rootEnd; ++rootIt) {
        if (rootIt->empty()) continue;
        if (candIt == candEnd || *rootIt != *candIt) return false;
        ++candIt;
    }
    return true;
}

bool IsImageExtension(const fs::path& path) {
    return ExtensionIn(path, { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd", ".hdr", ".qoi", ".dds" });
}

bool IsMtlExtension(const fs::path& path) {
    return ExtensionIn(path, { ".mtl" });
}

bool IsMediaSourceExtension(const fs::path& path) {
    return IsImageExtension(path) || IsMtlExtension(path) ||
           ExtensionIn(path, { ".obj", ".gltf", ".glb", ".iqm", ".wav", ".mp3", ".ogg", ".flac", ".xm", ".mod",
                               ".ttf", ".otf", ".mid", ".midi", ".sf2" });
}

fs::path ResolveMtlReference(const fs::path& baseDir, const std::string& reference) {
    return ResolveReference(baseDir, reference, &IsMtlExtension);
}

fs::path ResolveTextureReference(const fs::path& baseDir, const std::string& reference) {
    return ResolveReference(baseDir, reference, &IsImageExtension);
}

}
