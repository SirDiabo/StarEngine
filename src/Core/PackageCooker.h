#pragma once
#include "PackageCipher.h"
#include <string>

struct CookResult {
    bool success = false;
    std::string error;
    int filesCooked = 0;
};

namespace PackageCooker {
    CookResult Cook(const std::string& contentRoot, const std::string& outputContentDir,
                     CookCipher cipher, const std::string& cipherKey);
}
