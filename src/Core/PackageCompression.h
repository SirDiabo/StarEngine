#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PackCompressions {
    inline const std::string None = "None";
    inline const std::string Deflate = "Deflate";
}

class IPackCompressor {
public:
    virtual ~IPackCompressor() = default;

    virtual bool Compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const = 0;
    virtual bool Decompress(const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output) const = 0;
};

struct PackCompressionInfo {
    std::string id;
    std::string displayName;
    std::string description;
    std::string moduleName;
    std::shared_ptr<IPackCompressor> compressor;
};

class PackCompressionRegistry {
public:
    static PackCompressionRegistry& Get();

    void Register(const PackCompressionInfo& info);
    bool Find(const std::string& id, PackCompressionInfo& out) const;
    bool IsAvailable(const PackCompressionInfo& info) const;
    std::vector<PackCompressionInfo> GetAll() const;

private:
    PackCompressionRegistry();

    mutable std::mutex mutex;
    std::vector<PackCompressionInfo> methods;
};

namespace PackageCompression {
    bool Compress(const std::string& id, const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    bool Decompress(const std::string& id, const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output);
}
