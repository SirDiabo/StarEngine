#include "CookedFile.h"
#include "ContentPathResolver.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>

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

    std::ifstream in(resolved, std::ios::binary);
    if (!in.is_open()) return resolved;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    CreateCookCipher(activeCipher)->Transform(data, activeKey);

    fs::path tempDir = fs::temp_directory_path() / "StarEngineGameCache";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    size_t hash = std::hash<std::string>{}(resolved);
    fs::path tempPath = tempDir / (std::to_string(hash) + fs::path(resolved).extension().string());

    std::ofstream out(tempPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    out.close();

    decipheredPaths.emplace(resolved, tempPath.string());
    return tempPath.string();
}
