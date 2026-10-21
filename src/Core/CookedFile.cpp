#include "CookedFile.h"
#include "ContentPathResolver.h"
#include "PackageCompression.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <functional>
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
            if (!cipherImpl) {
                TraceLog(LOG_ERROR, "CookedFile: encryption method '%s' is not available. Is the module that provides it enabled?", activeCipher.c_str());
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
}

void CookedFile::SetActivePak(StarPak::Reader&& pak, const std::string& cipher, const std::string& key) {
    activePak = std::move(pak);
    usingPak = true;
    activeCipher = cipher.empty() ? CookCiphers::None : cipher;
    activeKey = key;
    decipheredPaths.clear();
    extractedDirs.clear();
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

    fs::path tempDir = fs::temp_directory_path() / "StarEngineGameCache";
    size_t dirHash = std::hash<std::string>{}(sourceDir.string());
    fs::path mirroredDir = tempDir / std::to_string(dirHash);

    if (!fs::exists(mirroredDir, ec)) {
        auto looseCipher = CreateCookCipher(activeCipher);
        if (!looseCipher) {
            TraceLog(LOG_ERROR, "CookedFile: encryption method '%s' is not available. Is the module that provides it enabled?", activeCipher.c_str());
            return resolved;
        }
        fs::create_directories(mirroredDir, ec);
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
