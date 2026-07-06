#include "PackageCipher.h"

namespace {

class NullCipher : public ICookCipher {
public:
    void Transform(std::vector<uint8_t>&, const std::string&) const override {}
};

class XorSimpleCipher : public ICookCipher {
public:
    void Transform(std::vector<uint8_t>& data, const std::string& key) const override {
        if (key.empty() || data.empty()) return;
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = (uint8_t)(data[i] ^ (uint8_t)key[i % key.size()]);
        }
    }
};

} // namespace

std::vector<CookCipher> AllCookCiphers() {
    return { CookCipher::None, CookCipher::XorSimple };
}

std::string CookCipherName(CookCipher cipher) {
    switch (cipher) {
        case CookCipher::XorSimple: return "XorSimple";
        case CookCipher::None:      return "None";
    }
    return "None";
}

std::string CookCipherDisplayName(CookCipher cipher) {
    switch (cipher) {
        case CookCipher::XorSimple: return "XOR (simple, unsafe)";
        case CookCipher::None:      return "None";
    }
    return "None";
}

CookCipher CookCipherFromName(const std::string& name) {
    if (name == "XorSimple") return CookCipher::XorSimple;
    return CookCipher::None;
}

std::unique_ptr<ICookCipher> CreateCookCipher(CookCipher cipher) {
    if (cipher == CookCipher::XorSimple) return std::make_unique<XorSimpleCipher>();
    return std::make_unique<NullCipher>();
}
