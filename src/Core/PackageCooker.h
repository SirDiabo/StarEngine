#pragma once
#include "PackageCipher.h"
#include "PackageCompression.h"
#include <cstdint>
#include <string>
#include <vector>

struct CookResult {
    bool success = false;
    std::string error;
    int filesCooked = 0;
    int filesCompressed = 0;
    int filesHashed = 0;
    uint64_t rawBytes = 0;
    uint64_t storedBytes = 0;
};

namespace PackageCooker {
    CookResult CookLoose(const std::string& contentRoot, const std::string& outputContentDir,
                          const std::string& cipher, const std::string& cipherKey);

    struct ExtraPakFile {
        std::string virtualPath;
        std::vector<uint8_t> bytes;
    };

    CookResult CookToPak(const std::string& contentRoot, const std::string& outputPakFile,
                         const std::string& cipher, const std::string& cipherKey, const std::string& bootConfigText,
                         const std::vector<ExtraPakFile>& extraFiles = {},
                         const std::string& compression = PackCompressions::None,
                         bool hashEntries = true);
}
