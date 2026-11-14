#include "CookedFile.h"
#include "ContentPathResolver.h"
#include "PackageCompression.h"
#include "raylib.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
    bool usingPak = false;
    std::string activeCipher = CookCiphers::None;
    std::string activeKey;

    StarPak::Reader activePak;
    std::unordered_map<std::string, std::string> decipheredPaths;
    std::unordered_set<std::string> extractedDirs;

    fs::path sessionDir;
    std::unordered_map<std::string, fs::path> mirrorDirs;
    size_t nextMirrorIndex = 0;

    void CleanupSessionDir() {
        if (sessionDir.empty()) return;
        std::error_code ec;
        fs::remove_all(sessionDir, ec);
        sessionDir.clear();
    }

    void SweepOldCaches(const fs::path& tempRoot) {
        std::error_code ec;
        fs::remove_all(tempRoot / "StarEngineGameCache", ec);
        ec.clear();

        const auto now = fs::file_time_type::clock::now();
        for (const auto& entry : fs::directory_iterator(tempRoot, ec)) {
            std::error_code entryEc;
            if (!entry.is_directory(entryEc)) continue;
            if (entry.path().filename().string().rfind("StarEngineGame_", 0) != 0) continue;
            const auto written = fs::last_write_time(entry.path(), entryEc);
            if (entryEc) continue;
            if (now - written > std::chrono::hours(24 * 7)) fs::remove_all(entry.path(), entryEc);
        }
    }

    bool EnsureSessionDir() {
        if (!sessionDir.empty()) return true;

        std::error_code ec;
        const fs::path tempRoot = fs::temp_directory_path(ec);
        if (ec) return false;
        SweepOldCaches(tempRoot);

        std::mt19937_64 rng(((uint64_t)std::random_device{}() << 32) ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
        for (int attempt = 0; attempt < 16; attempt++) {
            char name[48];
            std::snprintf(name, sizeof(name), "StarEngineGame_%016llx", (unsigned long long)rng());
            const fs::path candidate = tempRoot / name;
            ec.clear();
            if (fs::create_directory(candidate, ec) && !ec) {
                fs::permissions(candidate, fs::perms::owner_all, fs::perm_options::replace, ec);
                sessionDir = candidate;
                std::atexit(CleanupSessionDir);
                return true;
            }
        }
        return false;
    }

    fs::path MirrorDirFor(const std::string& key) {
        auto it = mirrorDirs.find(key);
        if (it != mirrorDirs.end()) return it->second;
        if (!EnsureSessionDir()) return {};

        const fs::path dir = sessionDir / ("d" + std::to_string(nextMirrorIndex++));
        std::error_code ec;
        fs::create_directory(dir, ec);
        if (ec) return {};
        mirrorDirs.emplace(key, dir);
        return dir;
    }

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
            fs::path mirroredDir = MirrorDirFor("pak:" + parent);
            if (mirroredDir.empty()) {
                TraceLog(LOG_ERROR, "CookedFile: could not create a private cache folder for %s", virtualPath.c_str());
                return "";
            }

            auto cipherImpl = CreateCookCipher(activeCipher);
            if (!cipherImpl) {
                TraceLog(LOG_ERROR, "CookedFile: obfuscation method '%s' is not available. Is the module that provides it enabled?", activeCipher.c_str());
                return "";
            }
            for (const std::string& sibling : activePak.SiblingsOf(virtualPath)) {
                if (ParentOf(sibling) != parent) continue;

                StarPak::Entry info;
                if (!activePak.GetEntryInfo(sibling, info)) continue;

                std::vector<uint8_t> bytes;
                std::string readError;
                if (!activePak.ReadEntryVerified(sibling, bytes, readError)) {
                    TraceLog(LOG_ERROR, "CookedFile: could not read %s: %s", sibling.c_str(), readError.c_str());
                    continue;
                }
                cipherImpl->Decrypt(bytes, activeKey, sibling);

                if (info.compression != StarPak::kNoCompression) {
                    std::vector<uint8_t> raw;
                    if (!PackageCompression::Decompress(info.compression, bytes, info.rawSize, raw)) {
                        TraceLog(LOG_ERROR, "CookedFile: could not decompress %s (method '%s' missing or data damaged)", sibling.c_str(), info.compression.c_str());
                        continue;
                    }
                    bytes.swap(raw);
                }

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

void CookedFile::SetActiveCipher(const std::string& cipher, const std::string& key) {
    usingPak = false;
    activeCipher = cipher.empty() ? CookCiphers::None : cipher;
    activeKey = key;
    decipheredPaths.clear();
    extractedDirs.clear();
    mirrorDirs.clear();
}

void CookedFile::SetActivePak(StarPak::Reader&& pak, const std::string& cipher, const std::string& key) {
    activePak = std::move(pak);
    usingPak = true;
    activeCipher = cipher.empty() ? CookCiphers::None : cipher;
    activeKey = key;
    decipheredPaths.clear();
    extractedDirs.clear();
    mirrorDirs.clear();
}

std::string CookedFile::Open(const std::string& path) {
    if (usingPak) return OpenFromPak(path);

    std::string resolved = ContentPathResolver::Resolve(path);
    if (activeCipher == CookCiphers::None || resolved.empty()) return resolved;

    auto cached = decipheredPaths.find(resolved);
    if (cached != decipheredPaths.end()) return cached->second;

    fs::path resolvedPath(resolved);
    fs::path sourceDir = resolvedPath.parent_path();
    std::error_code ec;
    if (!fs::exists(sourceDir, ec)) return resolved;

    const std::string mirrorKey = "loose:" + sourceDir.string();
    const bool alreadyMirrored = mirrorDirs.count(mirrorKey) > 0;
    fs::path mirroredDir = MirrorDirFor(mirrorKey);
    if (mirroredDir.empty()) return resolved;

    if (!alreadyMirrored) {
        auto looseCipher = CreateCookCipher(activeCipher);
        if (!looseCipher) {
            TraceLog(LOG_ERROR, "CookedFile: obfuscation method '%s' is not available. Is the module that provides it enabled?", activeCipher.c_str());
            return resolved;
        }
        for (const auto& entry : fs::directory_iterator(sourceDir, ec)) {
            if (!entry.is_regular_file()) continue;

            std::ifstream in(entry.path(), std::ios::binary);
            if (!in.is_open()) continue;
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            looseCipher->Decrypt(data, activeKey, ContentPathResolver::MakeRelative(entry.path().string()));

            std::ofstream out(mirroredDir / entry.path().filename(), std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        }
    }

    fs::path tempPath = mirroredDir / resolvedPath.filename();
    decipheredPaths.emplace(resolved, tempPath.string());
    return tempPath.string();
}
