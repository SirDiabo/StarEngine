#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace CollisionLayers {

constexpr int kMaxLayers = 32;
constexpr uint32_t kAllMask = 0xFFFFFFFFu;

void Load(const std::string& projectRoot);
void Save();

const std::string& NameAt(int index);
int IndexOf(const std::string& name);
std::vector<int> UsedIndices();
std::vector<std::string> Names();

int Add(const std::string& name);
bool Rename(int index, const std::string& name);
bool Remove(int index);

inline uint32_t Bit(int index) { return (index >= 0 && index < kMaxLayers) ? (1u << index) : 0u; }
uint32_t ParseMask(const std::string& text);
std::string MaskToString(uint32_t mask);

}
