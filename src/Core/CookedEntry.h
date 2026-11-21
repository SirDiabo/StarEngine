#pragma once
#include "StarPak.h"
#include <cstdint>
#include <string>
#include <vector>

namespace CookedEntry {

bool Read(const StarPak::Reader& pak, const std::string& cipherId, const std::string& cipherKey,
          const std::string& virtualPath, std::vector<uint8_t>& out, std::string& error);

}
