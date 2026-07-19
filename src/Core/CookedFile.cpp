#include "CookedFile.h"
#include "ContentPathResolver.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
    bool usingPak = false;
    CookCipher activeCipher = CookCipher::None;
    std::string activeKey;

    StarPak::Reader activePak;
    std::unordered_map<std::string, std::string> decipheredPaths;
    std::unordered_set<std::string> extractedDirs;

    std::string ParentOf(const std::string& virtualPath) {
        size_t slash = virtualPath.find_last_of('/');
        return slash == std::string::npos ? std::string() : virtualPath.substr(0, slash);
    }

    std::string Normalize(const std::string& path) {
        std::string out = path;
        for (char& c : out) if (c == '\\') c = '/';
        return out;
    }

    std::string OpenFromPak(const std::string& path) {
        std::string virtualPath = Normalize(path);

        auto cached = decipheredPaths.find(virtualPath);
        if (cached != decipheredPaths.end()) return cached->second;
        if (!activePak.HasEntry(virtualPath)) return "";

        std::string parent = ParentOf(virtualPath);
        if (!extractedDirs.count(parent)) {
            fs::path tempDir = fs::temp_directory_path() / "StarEngineGameCache";
            size_t dirHash = std::hash<std::string>{}(parent);
            fs::path mirroredDir = tempDir / std::to_string(dirHash);
            std::error_code ec;
            fs::create_directories(mirroredDir, ec);

            auto cipherImpl = CreateCookCipher(activeCipher);
            for (const std::string& sibling : activePak.SiblingsOf(virtualPath)) {
                if (ParentOf(sibling) != parent) continue;

                std::vector<uint8_t> bytes = activePak.ReadEntry(sibling);
                cipherImpl->Transform(bytes, activeKey);

                fs::path destPath = mirroredDir / fs::path(sibling).filename();
                std::ofstream out(destPath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());

                decipheredPaths.emplace(sibling, destPath.string());
            }
            extractedDirs.insert(parent);
        }

        auto resolved = decipheredPaths.find(virtualPath);
        return resolved != decipheredPaths.end() ? resolved->second : "";
    }
}

void CookedFile::SetActiveCipher(CookCipher cipher, const std::string& key) {
    usingPak = false;
    activeCipher = cipher;
    activeKey = key;
    decipheredPaths.clear();
    extractedDirs.clear();
}

void CookedFile::SetActivePak(StarPak::Reader&& pak, CookCipher cipher, const std::string& key) {
    activePak = std::move(pak);
    usingPak = true;
    activeCipher = cipher;
    activeKey = key;
    decipheredPaths.clear();
    extractedDirs.clear();
}

std::string CookedFile::Open(const std::string& path) {
    if (usingPak) return OpenFromPak(path);

    std::string resolved = ContentPathResolver::Resolve(path);
    if (activeCipher == CookCipher::None || resolved.empty()) return resolved;

    auto cached = decipheredPaths.find(resolved);
    if (cached != decipheredPaths.end()) return cached->second;

    fs::path resolvedPath(resolved);
    fs::path sourceDir = resolvedPath.parent_path();
    std::error_code ec;
    if (!fs::exists(sourceDir, ec)) return resolved;

    fs::path tempDir = fs::temp_directory_path() / "StarEngineGameCache";
    size_t dirHash = std::hash<std::string>{}(sourceDir.string());
    fs::path mirroredDir = tempDir / std::to_string(dirHash);

    if (!fs::exists(mirroredDir, ec)) {
        fs::create_directories(mirroredDir, ec);
        for (const auto& entry : fs::directory_iterator(sourceDir, ec)) {
            if (!entry.is_regular_file()) continue;

            std::ifstream in(entry.path(), std::ios::binary);
            if (!in.is_open()) continue;
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            CreateCookCipher(activeCipher)->Transform(data, activeKey);

            std::ofstream out(mirroredDir / entry.path().filename(), std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        }
    }

    fs::path tempPath = mirroredDir / resolvedPath.filename();
    decipheredPaths.emplace(resolved, tempPath.string());
    return tempPath.string();
}
