#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace StarPak {

constexpr const char* kBootConfigPath = "__boot__";
constexpr const char* kInputMappingsPath = "__input_mappings__";
constexpr const char* kIconPath = "__icon__";
constexpr const char* kAudioConfigPath = "__audio__";
constexpr const char* kNoCompression = "None";

struct Entry {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint64_t rawSize = 0;
    std::string compression = kNoCompression;
    std::string sha256;
    uint32_t crc32 = 0;
    bool hasCrc32 = false;
};

class Writer {
public:
    void SetIndexKey(const std::string& key) { indexKey = key; encryptIndex = true; }
    bool AddFile(const std::string& virtualPath, const std::vector<uint8_t>& bytes);
    bool AddEntry(const std::string& virtualPath, const std::vector<uint8_t>& storedBytes, uint64_t rawSize, const std::string& compression);
    bool WriteToFile(const std::string& outputPath) const;

private:
    std::vector<Entry> entries;
    std::vector<uint8_t> blob;
    std::string indexKey;
    bool encryptIndex = false;
};

class Reader {
public:
    bool LoadFile(const std::string& pakPath, const std::string& indexKey = std::string());
    bool LoadFromExecutable(const std::string& exePath, const std::string& indexKey = std::string());
    bool LoadBuffer(const std::vector<uint8_t>& buffer, const std::string& indexKey = std::string());

    bool HasEntry(const std::string& virtualPath) const;
    bool GetEntryInfo(const std::string& virtualPath, Entry& out) const;
    std::vector<uint8_t> ReadEntry(const std::string& virtualPath) const;
    bool CopyEntry(const std::string& virtualPath, std::vector<uint8_t>& out, Entry& info, std::string& error) const;
    bool VerifyEntryBytes(const Entry& info, const std::vector<uint8_t>& bytes, std::string& error) const;
    bool ReadEntryVerified(const std::string& virtualPath, std::vector<uint8_t>& out, std::string& error) const;
    void SetVerifyOnRead(bool enabled) { verifyOnRead = enabled; }


private:
    bool ParseFromBuffer(std::vector<uint8_t>&& buffer, const std::string& indexKey);

    std::vector<Entry> entries;
    std::unordered_map<std::string, size_t> indexByPath;
    std::vector<uint8_t> blob;
    bool verifyOnRead = true;
};

bool AppendPakToExecutable(const std::string& exePath, const std::string& pakPath);

}
