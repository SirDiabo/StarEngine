#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BakedKey {

struct Material {
    std::string cipher;
    std::string key;
};

constexpr size_t kMaxCipherName = 64;
constexpr size_t kMaxKeyBytes = 1024;
constexpr size_t kMinEntropyBytes = 16;

bool Encode(const Material& material, const std::vector<uint8_t>& entropy, std::vector<uint8_t>& blob);
bool Decode(const uint8_t* blob, size_t size, Material& out);
std::string BuildHeaderText(const std::vector<uint8_t>& blob);
std::string BuildEmptyHeaderText();
bool Get(Material& out);

}
