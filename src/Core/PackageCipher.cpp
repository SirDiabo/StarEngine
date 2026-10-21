#include "PackageCipher.h"
#include "Aes256.h"
#include "Module.h"
#include "Sha256.h"
#include <cstring>
#include <random>

namespace {

class NullCipher : public ICookCipher {
public:
    void Encrypt(std::vector<uint8_t>&, const std::string&, const std::string&) const override {}
    void Decrypt(std::vector<uint8_t>&, const std::string&, const std::string&) const override {}
};

class XorSimpleCipher : public ICookCipher {
public:
    void Encrypt(std::vector<uint8_t>& data, const std::string& key, const std::string&) const override { Apply(data, key); }
    void Decrypt(std::vector<uint8_t>& data, const std::string& key, const std::string&) const override { Apply(data, key); }

private:
    static void Apply(std::vector<uint8_t>& data, const std::string& key) {
        if (key.empty() || data.empty()) return;
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = (uint8_t)(data[i] ^ (uint8_t)key[i % key.size()]);
        }
    }
};

class Aes256CtrCipher : public ICookCipher {
public:
    void Encrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const override { Apply(data, key, context); }
    void Decrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const override { Apply(data, key, context); }

private:
    static void Apply(std::vector<uint8_t>& data, const std::string& key, const std::string& context) {
        if (data.empty()) return;

        Sha256::Digest keyBytes = Sha256::Compute(key);
        std::string nonceInput = "StarPak-CTR:" + context;
        Sha256::Digest nonce = Sha256::Hmac(keyBytes.bytes, 32, nonceInput.data(), nonceInput.size());

        Aes256::Key expanded;
        Aes256::ExpandKey(keyBytes.bytes, expanded);
        Aes256::CtrXor(expanded, nonce.bytes, data.data(), data.size());
    }
};

std::string RandomAlphanumericKey(size_t length) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, (int)sizeof(alphabet) - 2);
    std::string key(length, ' ');
    for (char& c : key) c = alphabet[dist(rd)];
    return key;
}

std::string RandomHexKey(size_t byteCount) {
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::string key;
    key.reserve(byteCount * 2);
    for (size_t i = 0; i < byteCount; i++) {
        uint32_t value = rd();
        key += hex[(value >> 4) & 15];
        key += hex[value & 15];
    }
    return key;
}

}

CookCipherRegistry& CookCipherRegistry::Get() {
    static CookCipherRegistry instance;
    return instance;
}

CookCipherRegistry::CookCipherRegistry() {
    CookCipherInfo none;
    none.id = CookCiphers::None;
    none.displayName = "None";
    none.description = "Content is stored as it is.";
    none.usesKey = false;
    none.create = []() { return std::unique_ptr<ICookCipher>(new NullCipher()); };
    ciphers.push_back(none);

    CookCipherInfo xorCipher;
    xorCipher.id = CookCiphers::XorSimple;
    xorCipher.displayName = "XOR (simple, unsafe)";
    xorCipher.description = "A fresh key is generated for every package. XOR deters casual inspection, it is not secure.";
    xorCipher.create = []() { return std::unique_ptr<ICookCipher>(new XorSimpleCipher()); };
    xorCipher.generateKey = []() { return RandomAlphanumericKey(24); };
    ciphers.push_back(xorCipher);

    CookCipherInfo aes;
    aes.id = CookCiphers::Aes256Ctr;
    aes.displayName = "AES-256 (CTR)";
    aes.description = "AES-256 with a fresh random key per package and a unique nonce per file. The key ships inside the game.";
    aes.create = []() { return std::unique_ptr<ICookCipher>(new Aes256CtrCipher()); };
    aes.generateKey = []() { return RandomHexKey(32); };
    ciphers.push_back(aes);
}

void CookCipherRegistry::Register(const CookCipherInfo& info) {
    if (info.id.empty() || !info.create) return;
    std::lock_guard<std::mutex> lock(mutex);
    for (CookCipherInfo& existing : ciphers) {
        if (existing.id == info.id) {
            existing = info;
            return;
        }
    }
    ciphers.push_back(info);
}

bool CookCipherRegistry::Find(const std::string& id, CookCipherInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex);
    for (const CookCipherInfo& info : ciphers) {
        if (info.id == id) {
            out = info;
            return true;
        }
    }
    return false;
}

bool CookCipherRegistry::IsAvailable(const CookCipherInfo& info) const {
    return info.moduleName.empty() || ModuleManager::Get().IsModuleEnabled(info.moduleName);
}

std::vector<CookCipherInfo> CookCipherRegistry::GetAll() const {
    std::lock_guard<std::mutex> lock(mutex);
    return ciphers;
}

std::unique_ptr<ICookCipher> CreateCookCipher(const std::string& id) {
    CookCipherInfo info;
    if (!CookCipherRegistry::Get().Find(id.empty() ? CookCiphers::None : id, info)) return nullptr;
    return info.create();
}

std::string GenerateCookKey(const std::string& id) {
    CookCipherInfo info;
    if (!CookCipherRegistry::Get().Find(id, info) || !info.usesKey) return "";
    if (info.generateKey) return info.generateKey();
    return RandomAlphanumericKey(24);
}
