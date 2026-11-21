#include "CookedEntry.h"
#include "PackageCipher.h"
#include "PackageCompression.h"
#include "Profiler.h"

bool CookedEntry::Read(const StarPak::Reader& pak, const std::string& cipherId, const std::string& cipherKey,
                       const std::string& virtualPath, std::vector<uint8_t>& out, std::string& error) {
    out.clear();

    StarPak::Entry info;
    {
        PROFILE_SCOPE("Pak Read");
        if (!pak.CopyEntry(virtualPath, out, info, error)) return false;
    }
    {
        PROFILE_SCOPE("Pak CRC Check");
        if (!pak.VerifyEntryBytes(info, out, error)) {
            out.clear();
            return false;
        }
    }
    {
        PROFILE_SCOPE("Pak Decrypt");
        std::unique_ptr<ICookCipher> cipher = CreateCookCipher(cipherId);
        if (!cipher) {
            out.clear();
            error = "obfuscation method '" + cipherId + "' is not available";
            return false;
        }
        cipher->Decrypt(out, cipherKey, virtualPath);
    }
    if (info.compression != StarPak::kNoCompression) {
        PROFILE_SCOPE("Pak Inflate");
        std::vector<uint8_t> raw;
        if (!PackageCompression::Decompress(info.compression, out, info.rawSize, raw)) {
            out.clear();
            error = "could not decompress (method '" + info.compression + "' missing or data damaged)";
            return false;
        }
        out.swap(raw);
    }
    return true;
}
