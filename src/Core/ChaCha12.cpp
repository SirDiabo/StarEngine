#include "ChaCha12.h"
#include <cstring>

namespace {

inline uint32_t Rotl(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

inline uint32_t Load32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline void Store32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

inline void QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = Rotl(d, 16);
    c += d; b ^= c; b = Rotl(b, 12);
    a += b; d ^= a; d = Rotl(d, 8);
    c += d; b ^= c; b = Rotl(b, 7);
}

void InitState(uint32_t state[16], const uint8_t key[ChaCha12::kKeySize], const uint8_t nonce[ChaCha12::kNonceSize]) {
    state[0] = 0x61707865u;
    state[1] = 0x3320646eu;
    state[2] = 0x79622d32u;
    state[3] = 0x6b206574u;
    for (int i = 0; i < 8; i++) state[4 + i] = Load32(key + 4 * i);
    state[12] = 0;
    state[13] = 0;
    state[14] = Load32(nonce);
    state[15] = Load32(nonce + 4);
}

void RunBlock(uint32_t state[16], uint64_t counter, int rounds, uint8_t out[ChaCha12::kBlockSize]) {
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32);

    uint32_t x[16];
    std::memcpy(x, state, sizeof(x));
    for (int round = 0; round < rounds / 2; round++) {
        QuarterRound(x[0], x[4], x[8], x[12]);
        QuarterRound(x[1], x[5], x[9], x[13]);
        QuarterRound(x[2], x[6], x[10], x[14]);
        QuarterRound(x[3], x[7], x[11], x[15]);
        QuarterRound(x[0], x[5], x[10], x[15]);
        QuarterRound(x[1], x[6], x[11], x[12]);
        QuarterRound(x[2], x[7], x[8], x[13]);
        QuarterRound(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; i++) Store32(out + 4 * i, x[i] + state[i]);
}

}

bool ChaCha12::BlockWithRounds(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, int rounds, uint8_t out[kBlockSize]) {
    if (rounds < 2 || rounds > 20 || (rounds % 2) != 0) return false;
    uint32_t state[16];
    InitState(state, key, nonce);
    RunBlock(state, counter, rounds, out);
    return true;
}

void ChaCha12::Block(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, uint8_t out[kBlockSize]) {
    uint32_t state[16];
    InitState(state, key, nonce);
    RunBlock(state, counter, kRounds, out);
}

void ChaCha12::Xor(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint64_t counter, uint8_t* data, size_t size) {
    if (size == 0 || data == nullptr) return;

    uint32_t state[16];
    InitState(state, key, nonce);

    uint8_t stream[kBlockSize];
    while (size > 0) {
        RunBlock(state, counter, kRounds, stream);
        counter++;
        const size_t take = size < kBlockSize ? size : kBlockSize;
        for (size_t i = 0; i < take; i++) data[i] = (uint8_t)(data[i] ^ stream[i]);
        data += take;
        size -= take;
    }
}
