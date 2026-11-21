#pragma once
#include <cstddef>
#include <cstdint>

namespace ChaCha12 {

constexpr size_t kKeySize = 32;
constexpr size_t kNonceSize = 8;
constexpr size_t kBlockSize = 64;
constexpr int kRounds = 12;

bool BlockWithRounds(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, int rounds, uint8_t out[kBlockSize]);
void Block(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, uint8_t out[kBlockSize]);
void Xor(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, uint8_t* data, size_t size);

}
