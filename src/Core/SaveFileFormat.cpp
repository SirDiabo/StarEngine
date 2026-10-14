#include "SaveFileFormat.h"
#include "SavePaths.h"
#include "SaveFileRegistry.h"
#include "PackageCipher.h"
#include "Project.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

namespace {
    std::unordered_map<std::string, std::string> ParseEntries(const std::string& text) {
        std::unordered_map<std::string, std::string> entries;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            entries[line.substr(0, separator)] = line.substr(separator + 1);
        }
        return entries;
    }

    std::string SerializeEntries(const std::unordered_map<std::string, std::string>& entries) {
        std::string text;
        for (const auto& kv : entries) {
            text += kv.first;
            text += "=";
            text += kv.second;
            text += "\n";
        }
        return text;
    }

    std::vector<uint8_t> ReadRawFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return {};
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    void WriteRawFile(const std::string& path, const std::vector<uint8_t>& data) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return;
        out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    }
}

namespace {

using EntryMap = std::unordered_map<std::string, std::string>;

struct CachedFile {
    EntryMap entries;
    bool dirty = false;
    bool encrypted = false;
    std::string encryptionKey;
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

    EntryMap Read(const std::string& path, bool encrypted, const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        return Load(path, encrypted, key).entries;
    }

    void Write(const std::string& path, bool encrypted, const std::string& key, const EntryMap& updates) {
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            CachedFile& file = Load(path, encrypted, key);
            for (const auto& kv : updates) file.entries[kv.first] = kv.second;
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

    void FlushDirty() {
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
                    auto cipher = CreateCookCipher(CookCipher::XorSimple);
                    cipher->Transform(raw, file.encryptionKey);
                }
                jobs.emplace_back(kv.first, std::move(raw));
                file.dirty = false;
            }
            pending = false;
        }
        for (const auto& job : jobs) {
            std::error_code ec;
            fs::create_directories(fs::path(job.first).parent_path(), ec);
            WriteRawFile(job.first, job.second);
        }
    }

    void Invalidate() {
        FlushDirty();
        std::lock_guard<std::mutex> lock(cacheMutex);
        files.clear();
    }

private:
    CachedFile& Load(const std::string& path, bool encrypted, const std::string& key) {
        auto it = files.find(path);
        if (it != files.end()) return it->second;

        CachedFile file;
        std::vector<uint8_t> raw = ReadRawFile(path);
        if (!raw.empty()) {
            if (encrypted) {
                auto cipher = CreateCookCipher(CookCipher::XorSimple);
                cipher->Transform(raw, key);
            }
            file.entries = ParseEntries(std::string(raw.begin(), raw.end()));
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
    return Writer().Read(SavePaths::GetSaveFilePath(saveFileName),
                         SaveFileRegistry::Get().IsEncrypted(saveFileName),
                         Project::Get().GetSettings().saveEncryptionKey);
}

bool SaveFileFormat::WriteEntries(const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates) {
    Writer().Write(SavePaths::GetSaveFilePath(saveFileName),
                   SaveFileRegistry::Get().IsEncrypted(saveFileName),
                   Project::Get().GetSettings().saveEncryptionKey, updates);
    return true;
}

void SaveFileFormat::FlushAll() {
    Writer().FlushDirty();
}

void SaveFileFormat::InvalidateCache() {
    Writer().Invalidate();
}
