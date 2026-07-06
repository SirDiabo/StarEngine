#pragma once
#include "PackageCipher.h"
#include <string>

namespace CookedFile {
    void SetActiveCipher(CookCipher cipher, const std::string& key);
    std::string Open(const std::string& path);
}
