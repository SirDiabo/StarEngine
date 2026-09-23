#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace StarPak {

constexpr const char* kBootConfigPath = "__boot__";
constexpr const char* kInputMappingsPath = "__input_mappings__";

struct Entry {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = 0;
};

class Writer {
public:
    void AddFile(const std::string& virtualPath, const std::vector<uint8_t>& bytes);
    bool WriteToFile(const std::string& outputPath) const;

private:
    std::vector<Entry> entries;
    std::vector<uint8_t> blob;
};

class Reader {
public:
    bool LoadFile(const std::string& pakPath);
    bool LoadFromExecutable(const std::string& exePath);
    bool LoadBuffer(const std::vector<uint8_t>& buffer);

    bool HasEntry(const std::string& virtualPath) const;
    std::vector<uint8_t> ReadEntry(const std::string& virtualPath) const;

    std::vector<std::string> SiblingsOf(const std::string& virtualPath) const;

private:
    bool ParseFromBuffer(std::vector<uint8_t>&& buffer);

    std::vector<Entry> entries;
    std::unordered_map<std::string, size_t> indexByPath;
    std::vector<uint8_t> blob;
};

bool AppendPakToExecutable(const std::string& exePath, const std::string& pakPath);

}
