#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace CookCiphers {
    inline const std::string None = "None";
    inline const std::string XorSimple = "XorSimple";
    inline const std::string Aes256Ctr = "Aes256Ctr";
}

class ICookCipher {
public:
    virtual ~ICookCipher() = default;

    virtual void Encrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const = 0;
    virtual void Decrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const = 0;
};

struct CookCipherInfo {
    std::string id;
    std::string displayName;
    std::string description;
    std::string moduleName;
    bool usesKey = true;
    std::function<std::unique_ptr<ICookCipher>()> create;
    std::function<std::string()> generateKey;
};

class CookCipherRegistry {
public:
    static CookCipherRegistry& Get();

    void Register(const CookCipherInfo& info);
    bool Find(const std::string& id, CookCipherInfo& out) const;
    bool IsAvailable(const CookCipherInfo& info) const;
    std::vector<CookCipherInfo> GetAll() const;

private:
    CookCipherRegistry();

    mutable std::mutex mutex;
    std::vector<CookCipherInfo> ciphers;
};

std::unique_ptr<ICookCipher> CreateCookCipher(const std::string& id);
std::string GenerateCookKey(const std::string& id);
