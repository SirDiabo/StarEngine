#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace StarPak {

constexpr const char* kBootConfigPath = "__boot__";
constexpr const char* kInputMappingsPath = "__input_mappings__";
constexpr const char* kIconPath = "__icon__";
constexpr const char* kNoCompression = "None";

struct Entry {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint64_t rawSize = 0;
    std::string compression = kNoCompression;
    std::string sha256;
};

class Writer {
public:
    void AddFile(const std::string& virtualPath, const std::vector<uint8_t>& bytes);
    void AddEntry(const std::string& virtualPath, const std::vector<uint8_t>& storedBytes, uint64_t rawSize, const std::string& compression);
    void SetHashEntries(bool enabled) { hashEntries = enabled; }
    bool WriteToFile(const std::string& outputPath) const;

private:
    std::vector<Entry> entries;
    std::vector<uint8_t> blob;
    bool hashEntries = true;
};

class Reader {
public:
    bool LoadFile(const std::string& pakPath);
    bool LoadFromExecutable(const std::string& exePath);
    bool LoadBuffer(const std::vector<uint8_t>& buffer);

    bool HasEntry(const std::string& virtualPath) const;
    bool GetEntryInfo(const std::string& virtualPath, Entry& out) const;
    std::vector<uint8_t> ReadEntry(const std::string& virtualPath) const;
    bool ReadEntryVerified(const std::string& virtualPath, std::vector<uint8_t>& out, std::string& error) const;
    void SetVerifyOnRead(bool enabled) { verifyOnRead = enabled; }

    std::vector<std::string> SiblingsOf(const std::string& virtualPath) const;

private:
    bool ParseFromBuffer(std::vector<uint8_t>&& buffer);

    std::vector<Entry> entries;
    std::unordered_map<std::string, size_t> indexByPath;
    std::vector<uint8_t> blob;
    bool verifyOnRead = true;
};

bool AppendPakToExecutable(const std::string& exePath, const std::string& pakPath);

}
