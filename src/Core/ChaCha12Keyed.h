#pragma once
#include "ChaCha12.h"
#include "Sha256.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ChaCha12 {

inline void ApplyKeyed(uint8_t* data, size_t size, const std::string& key, const std::string& context) {
    if (size == 0 || data == nullptr) return;

    Sha256::Digest keyBytes = Sha256::Compute(key);
    const std::string nonceInput = "StarPak-ChaCha12:" + context;
    Sha256::Digest nonce = Sha256::Hmac(keyBytes.bytes, 32, nonceInput.data(), nonceInput.size());

    Xor(keyBytes.bytes, nonce.bytes, 0, data, size);
}

inline void ApplyKeyed(std::vector<uint8_t>& data, const std::string& key, const std::string& context) {
    ApplyKeyed(data.data(), data.size(), key, context);
}

}
