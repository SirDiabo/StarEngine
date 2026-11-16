#include "TrustStore.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace {

const char* const kTrustHeader = "StarEngineTrust1";
const size_t kMaxFileBytes = 1024 * 1024;
const size_t kMaxEntries = 10000;
const size_t kMaxKeyLength = 4096;
const size_t kTokenHexLength = 64;

bool HasControlCharacter(const std::string& text) {
    for (unsigned char c : text) {
        if (c < 32 || c == 127) return true;
    }
    return false;
}

bool ReadSmallFile(const std::string& path, size_t limit, std::string& out) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    uintmax_t size = fs::file_size(path, ec);
    if (ec || size > limit) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

bool WriteFileAtomically(const std::string& path, const std::string& content) {
    std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(content.data(), (std::streamsize)content.size());
        out.flush();
        if (!out.good()) return false;
    }
    std::error_code ec;
    fs::rename(temp, path, ec);
    if (ec) {
        std::error_code removeEc;
        fs::remove(temp, removeEc);
        return false;
    }
    return true;
}

bool IsHexToken(const std::string& text) {
    if (text.size() != kTokenHexLength) return false;
    for (char c : text) {
        bool digit = c >= '0' && c <= '9';
        bool lower = c >= 'a' && c <= 'f';
        if (!digit && !lower) return false;
    }
    return true;
}

std::string GenerateToken() {
    std::random_device rd;
    uint64_t clock = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    static const char digits[] = "0123456789abcdef";
    std::string token;
    token.reserve(kTokenHexLength);
    for (int word = 0; word < 8; word++) {
        uint32_t value = rd() ^ (uint32_t)(clock >> ((word % 2) * 16));
        for (int nibble = 0; nibble < 8; nibble++) token += digits[(value >> (nibble * 4)) & 15];
    }
    return token;
}

}

TrustStore::TrustStore(std::string directoryPath) : directory(std::move(directoryPath)) {}

std::string TrustStore::KeyFor(const std::string& projectRoot) {
    if (projectRoot.empty() || projectRoot.size() > kMaxKeyLength) return {};
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(fs::path(projectRoot), ec);
    if (ec || canonical.empty()) return {};

    std::string key = canonical.generic_u8string();
    while (key.size() > 1 && key.back() == '/') key.pop_back();
    if (key.empty() || key.size() > kMaxKeyLength || HasControlCharacter(key)) return {};
#ifdef _WIN32
    for (char& c : key) c = (char)std::tolower((unsigned char)c);
#endif
    return key;
}

std::string TrustStore::TrustFilePath() const {
    return (fs::path(directory) / "trusted_projects.cfg").string();
}

std::string TrustStore::TokenFilePath() const {
    return (fs::path(directory) / "machine_id").string();
}

std::vector<std::string> TrustStore::LoadKeys() const {
    std::vector<std::string> keys;
    std::string content;
    if (!ReadSmallFile(TrustFilePath(), kMaxFileBytes, content)) return keys;

    std::istringstream stream(content);
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            first = false;
            if (line != kTrustHeader) return {};
            continue;
        }
        if (line.empty() || line.size() > kMaxKeyLength || HasControlCharacter(line)) continue;
        if (keys.size() >= kMaxEntries) break;
        keys.push_back(line);
    }
    return keys;
}

bool TrustStore::SaveKeys(const std::vector<std::string>& keys) const {
    std::error_code ec;
    fs::create_directories(directory, ec);
    std::string content = std::string(kTrustHeader) + "\n";
    for (const std::string& key : keys) content += key + "\n";
    return WriteFileAtomically(TrustFilePath(), content);
}

std::vector<std::string> TrustStore::TrustedKeys() const {
    return LoadKeys();
}

bool TrustStore::IsTrusted(const std::string& projectRoot) const {
    std::string key = KeyFor(projectRoot);
    if (key.empty()) return false;
    std::vector<std::string> keys = LoadKeys();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

bool TrustStore::Trust(const std::string& projectRoot) {
    std::string key = KeyFor(projectRoot);
    if (key.empty()) return false;
    std::vector<std::string> keys = LoadKeys();
    if (std::find(keys.begin(), keys.end(), key) != keys.end()) return true;
    if (keys.size() >= kMaxEntries) return false;
    keys.push_back(key);
    return SaveKeys(keys);
}

bool TrustStore::Revoke(const std::string& projectRoot) {
    std::string key = KeyFor(projectRoot);
    if (key.empty()) return false;
    std::vector<std::string> keys = LoadKeys();
    auto it = std::remove(keys.begin(), keys.end(), key);
    if (it == keys.end()) return true;
    keys.erase(it, keys.end());
    return SaveKeys(keys);
}

std::string TrustStore::MachineToken() {
    if (!cachedToken.empty()) return cachedToken;

    std::string content;
    if (ReadSmallFile(TokenFilePath(), 256, content)) {
        while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' ')) content.pop_back();
        if (IsHexToken(content)) {
            cachedToken = content;
            return cachedToken;
        }
    }

    cachedToken = GenerateToken();
    std::error_code ec;
    fs::create_directories(directory, ec);
    WriteFileAtomically(TokenFilePath(), cachedToken + "\n");
    return cachedToken;
}
