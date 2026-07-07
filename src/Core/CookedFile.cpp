#include "CookedFile.h"
#include "ContentPathResolver.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
    CookCipher activeCipher = CookCipher::None;
    std::string activeKey;
    std::unordered_map<std::string, std::string> decipheredPaths;
}

void CookedFile::SetActiveCipher(CookCipher cipher, const std::string& key) {
    activeCipher = cipher;
    activeKey = key;
    decipheredPaths.clear();
}

std::string CookedFile::Open(const std::string& path) {
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
