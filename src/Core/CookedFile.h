#pragma once
#include "PackageCipher.h"
#include "StarPak.h"
#include <string>

namespace CookedFile {
    
    void SetActiveCipher(const std::string& cipher, const std::string& key);

    void SetActivePak(StarPak::Reader&& pak, const std::string& cipher, const std::string& key);

    std::string Open(const std::string& path);
}
