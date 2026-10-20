#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class PackCompression {
    None = 0,
    Deflate = 1,
};

std::vector<PackCompression> AllPackCompressions();
std::string PackCompressionName(PackCompression method);
std::string PackCompressionDisplayName(PackCompression method);
PackCompression PackCompressionFromName(const std::string& name);
inline bool IsKnownPackCompression(int id) {
    return id == (int)PackCompression::None || id == (int)PackCompression::Deflate;
}

namespace PackageCompression {
    bool Compress(PackCompression method, const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    bool Decompress(PackCompression method, const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output);
}
