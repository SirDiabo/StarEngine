#pragma once
#include <cstddef>
#include <cstdint>

namespace Aes256 {

struct Key {
    uint32_t roundKeys[60];
};

void ExpandKey(const uint8_t key[32], Key& out);
void EncryptBlock(const Key& key, const uint8_t in[16], uint8_t out[16]);
void CtrXor(const Key& key, const uint8_t counter[16], uint8_t* data, size_t size);

}
