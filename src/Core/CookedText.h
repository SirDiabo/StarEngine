#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CookedText {

constexpr size_t kMaxTextBytes = 256u * 1024u * 1024u;
constexpr size_t kMaxDictionaryWords = 1u << 22;

bool LooksCooked(const uint8_t* data, size_t size);
bool Pack(const std::string& text, uint32_t seed, std::vector<uint8_t>& out);
bool Unpack(const uint8_t* data, size_t size, std::string& out);

}
