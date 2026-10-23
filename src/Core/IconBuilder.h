#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace IconBuilder {

struct Result {
    bool success = false;
    std::string error;
    std::vector<uint8_t> png;
    std::vector<uint8_t> ico;
};

constexpr int kMaxIconSize = 256;
constexpr int kMinSourceSize = 16;

bool BuildFromFile(const std::string& path, Result& out);
bool BuildFromImageBytes(const std::vector<uint8_t>& bytes, const std::string& fileType, Result& out);
std::vector<uint8_t> AssembleIco(const std::vector<std::vector<uint8_t>>& pngImages, const std::vector<int>& sizes);
std::string ResolveSourcePath(const std::string& projectRoot, const std::string& configuredIcon, const std::string& engineRoot);
bool WriteBytes(const std::string& path, const std::vector<uint8_t>& bytes);

}
