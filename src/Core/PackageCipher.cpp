#include "PackageCipher.h"
#include "ChaCha12Keyed.h"
#include "Module.h"
#include "Sha256.h"
#include <chrono>
#include <cstring>
#include <random>

namespace {

class NullCipher : public ICookCipher {
public:
    void Encrypt(std::vector<uint8_t>&, const std::string&, const std::string&) const override {}
    void Decrypt(std::vector<uint8_t>&, const std::string&, const std::string&) const override {}
};

class ChaCha12Cipher : public ICookCipher {
public:
    void Encrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const override { ChaCha12::ApplyKeyed(data, key, context); }
    void Decrypt(std::vector<uint8_t>& data, const std::string& key, const std::string& context) const override { ChaCha12::ApplyKeyed(data, key, context); }
};

std::vector<uint8_t> RandomBytes(size_t count) {
    std::random_device rd;
    std::vector<uint8_t> out;
    out.reserve(count + 32);
    uint64_t block = 0;
    while (out.size() < count) {
        std::vector<uint8_t> material;
        material.reserve(16 * 4 + 24);
        for (int i = 0; i < 16; i++) {
            uint32_t word = rd();
            for (int b = 0; b < 4; b++) material.push_back((uint8_t)(word >> (8 * b)));
        }
        const uint64_t ticks = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const uint64_t address = (uint64_t)(uintptr_t)&material;
        for (int b = 0; b < 8; b++) material.push_back((uint8_t)(ticks >> (8 * b)));
        for (int b = 0; b < 8; b++) material.push_back((uint8_t)(address >> (8 * b)));
        for (int b = 0; b < 8; b++) material.push_back((uint8_t)(block >> (8 * b)));
        block++;
        Sha256::Digest digest = Sha256::Compute(material.data(), material.size());
        out.insert(out.end(), digest.bytes, digest.bytes + 32);
    }
    out.resize(count);
    return out;
}

std::string RandomAlphanumericKey(size_t length) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const size_t alphabetSize = sizeof(alphabet) - 1;
    const size_t limit = 256 - (256 % alphabetSize);
    std::string key;
    key.reserve(length);
    while (key.size() < length) {
        std::vector<uint8_t> bytes = RandomBytes(length * 2 + 16);
        for (uint8_t value : bytes) {
            if (value >= limit) continue;
            key += alphabet[value % alphabetSize];
            if (key.size() == length) break;
        }
    }
    return key;
}

std::string RandomHexKey(size_t byteCount) {
    static const char hex[] = "0123456789abcdef";
    std::vector<uint8_t> bytes = RandomBytes(byteCount);
    std::string key;
    key.reserve(byteCount * 2);
    for (uint8_t value : bytes) {
        key += hex[value >> 4];
        key += hex[value & 15];
    }
    return key;
}

}

std::vector<uint8_t> CookRandomBytes(size_t count) {
    return RandomBytes(count);
}

CookCipherRegistry& CookCipherRegistry::Get() {
    static CookCipherRegistry instance;
    return instance;
}

CookCipherRegistry::CookCipherRegistry() {
    CookCipherInfo none;
    none.id = CookCiphers::None;
    none.displayName = "None";
    none.description = "Content is stored as it is and can be read by anyone who has the package.";
    none.usesKey = false;
    none.create = []() { return std::unique_ptr<ICookCipher>(new NullCipher()); };
    ciphers.push_back(none);

    CookCipherInfo chacha;
    chacha.id = CookCiphers::ChaCha12;
    chacha.displayName = "ChaCha12";
    chacha.description = "Obfuscates content with ChaCha12, using a fresh random key for every package and a unique nonce per file. The key ships inside the game, so this deters casual extraction but is not copy protection.";
    chacha.create = []() { return std::unique_ptr<ICookCipher>(new ChaCha12Cipher()); };
    chacha.generateKey = []() { return RandomHexKey(32); };
    ciphers.push_back(chacha);
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
