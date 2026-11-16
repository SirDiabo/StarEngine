#include "BuildCacheGuard.h"
#include "PathSafety.h"
#include "Sha256.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

const size_t kMaxMarkerBytes = 512;

std::string NormalizedDirKey(const fs::path& dir) {
    std::string key = dir.generic_u8string();
    while (key.size() > 1 && key.back() == '/') key.pop_back();
#ifdef _WIN32
    for (char& c : key) c = (char)std::tolower((unsigned char)c);
#endif
    return key;
}

bool ReadMarker(const fs::path& markerPath, std::string& out) {
    std::error_code ec;
    if (!fs::is_regular_file(markerPath, ec)) return false;
    uintmax_t size = fs::file_size(markerPath, ec);
    if (ec || size > kMaxMarkerBytes) return false;
    std::ifstream in(markerPath, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

bool WriteMarker(const fs::path& markerPath, const std::string& marker) {
    std::ofstream out(markerPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(marker.data(), (std::streamsize)marker.size());
    out.flush();
    return out.good();
}

bool ClearAndMark(const fs::path& canonicalCache, const std::string& marker, bool& hadContents, std::string& error) {
    std::error_code ec;
    std::vector<fs::path> children;
    for (fs::directory_iterator it(canonicalCache, ec), end; !ec && it != end; it.increment(ec)) {
        children.push_back(it->path());
    }
    if (ec) {
        error = "Could not read the build cache folder: " + ec.message();
        return false;
    }

    hadContents = false;
    for (const fs::path& child : children) {
        if (child.filename() != ".starengine_cache") hadContents = true;
        std::error_code removeEc;
        fs::remove_all(child, removeEc);
        if (removeEc) {
            error = "Could not clear the old build cache: " + removeEc.message();
            return false;
        }
    }

    if (!WriteMarker(canonicalCache / ".starengine_cache", marker)) {
        error = "Could not write the build cache marker.";
        return false;
    }
    return true;
}

bool IsSymbolicLink(const fs::path& path) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(path, ec);
    return !ec && fs::is_symlink(status);
}

}

namespace BuildCacheGuard {

const char* MarkerFileName() {
    return ".starengine_cache";
}

std::string ExpectedMarker(const std::string& canonicalCacheDir, const std::string& machineToken) {
    std::string message = "StarEngine build cache|" + canonicalCacheDir;
    Sha256::Digest mac = Sha256::Hmac(reinterpret_cast<const uint8_t*>(machineToken.data()), machineToken.size(),
                                      reinterpret_cast<const uint8_t*>(message.data()), message.size());
    return "StarEngineBuildCache1\n" + Sha256::ToHex(mac) + "\n";
}

Preparation Prepare(const std::string& projectRoot, const std::string& cacheName, const std::string& machineToken) {
    Preparation result;

    if (!PathSafety::IsPlainFileName(cacheName)) {
        result.error = "Invalid build cache name.";
        return result;
    }
    if (machineToken.empty()) {
        result.error = "No machine token is available for the build cache.";
        return result;
    }

    std::error_code ec;
    fs::path root = fs::weakly_canonical(fs::path(projectRoot), ec);
    if (ec || projectRoot.empty() || !fs::is_directory(root, ec)) {
        result.error = "The project folder could not be found.";
        return result;
    }

    fs::path saved = root / "Saved";
    fs::path buildRoot = saved / "Build";
    fs::path cache = buildRoot / cacheName;

    for (const fs::path& part : { saved, buildRoot, cache }) {
        if (IsSymbolicLink(part)) {
            result.error = "\"" + part.string() + "\" is a symbolic link, so the build cache will not be placed there.";
            return result;
        }
    }

    fs::create_directories(cache, ec);
    if (ec) {
        result.error = "Could not create the build cache folder: " + ec.message();
        return result;
    }

    fs::path canonicalCache = fs::weakly_canonical(cache, ec);
    if (ec || !PathSafety::IsInside(root, canonicalCache) || canonicalCache == root) {
        result.error = "The build cache folder resolves to a location outside the project, so it will not be used.";
        return result;
    }

    fs::path markerPath = canonicalCache / MarkerFileName();
    std::string expected = ExpectedMarker(NormalizedDirKey(canonicalCache), machineToken);
    std::string existing;
    bool valid = ReadMarker(markerPath, existing) && existing == expected;

    if (!valid) {
        bool hadContents = false;
        if (!ClearAndMark(canonicalCache, expected, hadContents, result.error)) return result;
        result.wiped = hadContents;
    }

    result.ok = true;
    result.path = canonicalCache.string();
    return result;
}

bool Reset(const std::string& preparedCachePath, const std::string& machineToken, std::string& error) {
    std::error_code ec;
    fs::path cache = fs::weakly_canonical(fs::path(preparedCachePath), ec);
    if (ec || machineToken.empty() || !fs::is_directory(cache, ec) || IsSymbolicLink(cache)) {
        error = "The build cache folder is not available.";
        return false;
    }
    bool hadContents = false;
    return ClearAndMark(cache, ExpectedMarker(NormalizedDirKey(cache), machineToken), hadContents, error);
}

}
