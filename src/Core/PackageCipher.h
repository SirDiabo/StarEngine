#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class CookCipher {
    None,
    XorSimple,
};

std::vector<CookCipher> AllCookCiphers();
std::string CookCipherName(CookCipher cipher);
std::string CookCipherDisplayName(CookCipher cipher);
CookCipher CookCipherFromName(const std::string& name);

class ICookCipher {
public:
    virtual ~ICookCipher() = default;

    virtual void Transform(std::vector<uint8_t>& data, const std::string& key) const = 0;
};

std::unique_ptr<ICookCipher> CreateCookCipher(CookCipher cipher);
