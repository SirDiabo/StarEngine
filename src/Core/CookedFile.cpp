#include "CookedFile.h"
#include "ContentPathResolver.h"
#include "CookedEntry.h"
#include "CookedText.h"
#include "MemoryStream.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    bool usingPak = false;
    std::string activeCipher = CookCiphers::None;
    std::string activeKey;
    StarPak::Reader activePak;

    std::string Normalize(const std::string& path) {
        std::string out = path;
        for (char& c : out) if (c == '\\') c = '/';
        return out;
    }

    bool ReadLoose(const std::string& path, std::vector<uint8_t>& out) {
        const std::string disk = ContentPathResolver::Resolve(path);
        if (disk.empty()) return false;
        std::ifstream in(fs::path(disk), std::ios::binary);
        if (!in.is_open()) return false;
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return !in.bad();
    }

    bool ReadAny(const std::string& path, std::vector<uint8_t>& out) {
        out.clear();
        if (!usingPak) return ReadLoose(path, out);

        const std::string virtualPath = Normalize(path);
        if (!activePak.HasEntry(virtualPath)) return false;

        std::string error;
        if (!CookedEntry::Read(activePak, activeCipher, activeKey, virtualPath, out, error)) {
            TraceLog(LOG_ERROR, "CookedFile: could not read %s: %s", virtualPath.c_str(), error.c_str());
            out.clear();
            return false;
        }
        return true;
    }
}

void CookedFile::SetActivePak(StarPak::Reader&& pak, const std::string& cipher, const std::string& key) {
    activePak = std::move(pak);
    usingPak = true;
    activeCipher = cipher.empty() ? CookCiphers::None : cipher;
    activeKey = key;
}

bool CookedFile::IsUsingPak() {
    return usingPak;
}

std::string CookedFile::DiskPath(const std::string& path) {
    if (usingPak) return "";
    return ContentPathResolver::Resolve(path);
}

std::string CookedFile::Key(const std::string& path) {
    if (usingPak) return Normalize(path);
    std::string disk = ContentPathResolver::Resolve(path);
    return disk.empty() ? path : disk;
}

bool CookedFile::Exists(const std::string& path) {
    if (usingPak) return activePak.HasEntry(Normalize(path));
    const std::string disk = ContentPathResolver::Resolve(path);
    if (disk.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(fs::path(disk), ec);
}

long CookedFile::ModifiedTime(const std::string& path) {
    if (usingPak) return 0;
    const std::string disk = ContentPathResolver::Resolve(path);
    if (disk.empty()) return 0;
    return (long)GetFileModTime(disk.c_str());
}

std::shared_ptr<const std::vector<uint8_t>> CookedFile::ReadShared(const std::string& path) {
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    if (!ReadAny(path, *bytes)) return nullptr;
    return bytes;
}

bool CookedFile::ReadBytes(const std::string& path, std::vector<uint8_t>& out) {
    return ReadAny(path, out);
}

bool CookedFile::ReadText(const std::string& path, std::string& out) {
    out.clear();
    std::vector<uint8_t> bytes;
    if (!ReadAny(path, bytes)) return false;
    if (usingPak && CookedText::LooksCooked(bytes.data(), bytes.size())) {
        if (!CookedText::Unpack(bytes.data(), bytes.size(), out)) {
            TraceLog(LOG_ERROR, "CookedFile: %s is damaged", path.c_str());
            out.clear();
            return false;
        }
        return true;
    }
    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

std::unique_ptr<std::istream> CookedFile::OpenStream(const std::string& path) {
    std::string text;
    if (!ReadText(path, text)) return nullptr;
    auto bytes = std::make_shared<std::vector<uint8_t>>(text.begin(), text.end());
    return std::unique_ptr<std::istream>(new MemoryStream(bytes));
}
