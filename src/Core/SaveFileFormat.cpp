#include "SaveFileFormat.h"
#include "SavePaths.h"
#include "SaveFileRegistry.h"
#include "PackageCipher.h"
#include "Project.h"
#include "SaveSlots.h"
#include "SaveMigration.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {
    const char* const kFormatHeader = "#starsav 2";

    std::string EscapeText(const std::string& text, bool escapeEquals) {
        std::string out;
        out.reserve(text.size());
        for (char c : text) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '=': if (escapeEquals) out += "\\e"; else out += c; break;
                default: out += c;
            }
        }
        return out;
    }

    std::string UnescapeText(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] != '\\' || i + 1 >= text.size()) { out += text[i]; continue; }
            char next = text[i + 1];
            if (next == '\\') { out += '\\'; i++; }
            else if (next == 'n') { out += '\n'; i++; }
            else if (next == 'r') { out += '\r'; i++; }
            else if (next == 'e') { out += '='; i++; }
            else out += text[i];
        }
        return out;
    }

    std::unordered_map<std::string, std::string> ParseEntries(const std::string& text) {
        std::unordered_map<std::string, std::string> entries;
        std::istringstream in(text);
        std::string line;
        bool escaped = false;
        bool firstLine = true;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (firstLine) {
                firstLine = false;
                if (line == kFormatHeader) { escaped = true; continue; }
            }
            if (line.empty()) continue;

            size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            if (escaped) { key = UnescapeText(key); value = UnescapeText(value); }
            entries[key] = value;
        }
        return entries;
    }

    std::string SerializeEntries(const std::unordered_map<std::string, std::string>& entries) {
        std::vector<const std::pair<const std::string, std::string>*> ordered;
        ordered.reserve(entries.size());
        for (const auto& kv : entries) ordered.push_back(&kv);
        std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) { return a->first < b->first; });

        std::string text = kFormatHeader;
        text += "\n";
        for (const auto* kv : ordered) {
            text += EscapeText(kv->first, true);
            text += "=";
            text += EscapeText(kv->second, false);
            text += "\n";
        }
        return text;
    }

    std::vector<uint8_t> ReadRawFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return {};
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    bool SyncToDisk(FILE* file) {
#ifdef _WIN32
        return _commit(_fileno(file)) == 0;
#else
        return fsync(fileno(file)) == 0;
#endif
    }

    bool WriteRawFile(const std::string& path, const std::vector<uint8_t>& data) {
        const std::string tempPath = path + ".tmp";
        FILE* file = fopen(tempPath.c_str(), "wb");
        if (!file) return false;

        bool ok = data.empty() || fwrite(data.data(), 1, data.size(), file) == data.size();
        ok = (fflush(file) == 0) && ok;
        if (ok) ok = SyncToDisk(file);
        ok = (fclose(file) == 0) && ok;

        std::error_code ec;
        if (ok) {
            fs::rename(tempPath, path, ec);
            ok = !ec;
        }
        if (!ok) fs::remove(tempPath, ec);
        return ok;
    }
}

namespace {

using EntryMap = std::unordered_map<std::string, std::string>;

struct CachedFile {
    EntryMap entries;
    bool dirty = false;
    bool encrypted = false;
    std::string encryptionKey;
    int failedWrites = 0;
};

class SaveWriteBehind {
public:
    ~SaveWriteBehind() {
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            stopping = true;
        }
        wake.notify_all();
        if (worker.joinable()) worker.join();
        FlushDirty();
    }

    EntryMap Read(const std::string& path, const std::string& name, bool versioned, bool encrypted, const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        return Load(path, name, versioned, encrypted, key).entries;
    }

    void Write(const std::string& path, const std::string& name, bool versioned, bool encrypted, const std::string& key, const EntryMap& updates) {
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            CachedFile& file = Load(path, name, versioned, encrypted, key);
            for (const auto& kv : updates) file.entries[kv.first] = kv.second;
            if (versioned && SaveMigration::FileVersion(file.entries) == 0 && file.entries.count("@saveDataVersion") == 0) {
                SaveMigration::InjectVersion(file.entries);
            }
            file.dirty = true;
            file.encrypted = encrypted;
            file.encryptionKey = key;
            if (!pending) {
                pending = true;
                flushAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            }
            if (!worker.joinable()) worker = std::thread([this]() { Run(); });
        }
        wake.notify_all();
    }

    void FlushDirty(std::vector<std::string>* failedOut = nullptr) {
        std::lock_guard<std::mutex> writeLock(writeMutex);
        std::vector<std::pair<std::string, std::vector<uint8_t>>> jobs;
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            for (auto& kv : files) {
                CachedFile& file = kv.second;
                if (!file.dirty) continue;
                std::string text = SerializeEntries(file.entries);
                std::vector<uint8_t> raw(text.begin(), text.end());
                if (file.encrypted) {
                    auto cipher = CreateCookCipher(CookCiphers::XorSimple);
                    cipher->Encrypt(raw, file.encryptionKey, kv.first);
                }
                jobs.emplace_back(kv.first, std::move(raw));
                file.dirty = false;
            }
            pending = false;
        }
        std::vector<std::string> failed;
        std::vector<std::string> succeeded;
        for (const auto& job : jobs) {
            std::error_code ec;
            fs::create_directories(fs::path(job.first).parent_path(), ec);
            if (WriteRawFile(job.first, job.second)) succeeded.push_back(job.first);
            else failed.push_back(job.first);
        }

        if (!jobs.empty()) {
            std::lock_guard<std::mutex> lock(cacheMutex);
            for (const std::string& path : succeeded) {
                auto it = files.find(path);
                if (it != files.end()) it->second.failedWrites = 0;
            }
            for (const std::string& path : failed) {
                auto it = files.find(path);
                if (it == files.end()) continue;
                it->second.dirty = true;
                it->second.failedWrites++;
                if (it->second.failedWrites < kMaxWriteRetries) {
                    pending = true;
                    flushAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
            }
        }
        if (failedOut) *failedOut = std::move(failed);
    }

    void FlushAndReport() {
        std::vector<std::string> failed;
        FlushDirty(&failed);
        for (const std::string& path : failed) {
            TraceLog(LOG_ERROR, "SaveFileFormat: could not write '%s'; the previous save file was left untouched", path.c_str());
        }
    }

    void DiscardUnder(const std::string& directory) {
        FlushAndReport();
        std::lock_guard<std::mutex> lock(cacheMutex);
        for (auto it = files.begin(); it != files.end();) {
            const std::string& path = it->first;
            bool inside = path.size() > directory.size() && path.compare(0, directory.size(), directory) == 0 &&
                          (path[directory.size()] == '/' || path[directory.size()] == '\\');
            if (inside && path.find_first_of("/\\", directory.size() + 1) == std::string::npos) it = files.erase(it);
            else ++it;
        }
    }

    void Invalidate() {
        FlushAndReport();
        std::lock_guard<std::mutex> lock(cacheMutex);
        for (auto it = files.begin(); it != files.end();) {
            if (it->second.dirty) ++it;
            else it = files.erase(it);
        }
    }

private:
    CachedFile& Load(const std::string& path, const std::string& name, bool versioned, bool encrypted, const std::string& key) {
        auto it = files.find(path);
        if (it != files.end()) return it->second;

        CachedFile file;
        std::vector<uint8_t> raw = ReadRawFile(path);
        if (!raw.empty()) {
            if (encrypted) {
                auto cipher = CreateCookCipher(CookCiphers::XorSimple);
                cipher->Decrypt(raw, key, path);
            }
            file.entries = ParseEntries(std::string(raw.begin(), raw.end()));
            if (versioned && !file.entries.empty() && SaveMigration::Migrate(name, file.entries)) file.dirty = true;
        }
        return files.emplace(path, std::move(file)).first->second;
    }

    void Run() {
        std::unique_lock<std::mutex> lock(cacheMutex);
        while (!stopping) {
            if (!pending) {
                wake.wait(lock);
                continue;
            }
            if (std::chrono::steady_clock::now() < flushAt) {
                wake.wait_until(lock, flushAt);
                continue;
            }
            lock.unlock();
            FlushDirty();
            lock.lock();
        }
    }

    static constexpr int kMaxWriteRetries = 5;

    std::mutex cacheMutex;
    std::mutex writeMutex;
    std::condition_variable wake;
    std::unordered_map<std::string, CachedFile> files;
    std::thread worker;
    std::chrono::steady_clock::time_point flushAt;
    bool pending = false;
    bool stopping = false;
};

SaveWriteBehind& Writer() {
    static SaveWriteBehind instance;
    return instance;
}

}

std::unordered_map<std::string, std::string> SaveFileFormat::ReadEntries(const std::string& saveFileName) {
    return ReadEntriesInSlot(SavePaths::GetCurrentSlot(), saveFileName);
}

bool SaveFileFormat::WriteEntries(const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates) {
    bool written = WriteEntriesInSlot(SavePaths::GetCurrentSlot(), saveFileName, updates);
    if (written && !SaveSlots::IsMetadataFile(saveFileName)) SaveSlots::NotifySaved();
    return written;
}

std::unordered_map<std::string, std::string> SaveFileFormat::ReadEntriesInSlot(const std::string& slot, const std::string& saveFileName) {
    return Writer().Read(SavePaths::GetSaveFilePathInSlot(slot, saveFileName), saveFileName,
                         !SaveSlots::IsMetadataFile(saveFileName),
                         SaveFileRegistry::Get().IsEncrypted(saveFileName),
                         Project::Get().GetSettings().saveEncryptionKey);
}

bool SaveFileFormat::WriteEntriesInSlot(const std::string& slot, const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates) {
    Writer().Write(SavePaths::GetSaveFilePathInSlot(slot, saveFileName), saveFileName,
                   !SaveSlots::IsMetadataFile(saveFileName),
                   SaveFileRegistry::Get().IsEncrypted(saveFileName),
                   Project::Get().GetSettings().saveEncryptionKey, updates);
    return true;
}

void SaveFileFormat::DiscardSlotCache(const std::string& slot) {
    Writer().DiscardUnder(SavePaths::GetSlotDirectory(slot));
}

void SaveFileFormat::FlushAll() {
    Writer().FlushAndReport();
}

void SaveFileFormat::InvalidateCache() {
    Writer().Invalidate();
}
