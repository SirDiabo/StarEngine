#include "Aes256.h"
#include <cstring>

namespace {

struct Tables {
    uint8_t sbox[256];
    uint32_t te[4][256];

    Tables() {
        uint8_t p = 1;
        uint8_t q = 1;
        do {
            p = (uint8_t)(p ^ (uint8_t)(p << 1) ^ ((p & 0x80) ? 0x1B : 0));
            q = (uint8_t)(q ^ (uint8_t)(q << 1));
            q = (uint8_t)(q ^ (uint8_t)(q << 2));
            q = (uint8_t)(q ^ (uint8_t)(q << 4));
            if (q & 0x80) q = (uint8_t)(q ^ 0x09);
            uint8_t x = (uint8_t)(q ^ RotL8(q, 1) ^ RotL8(q, 2) ^ RotL8(q, 3) ^ RotL8(q, 4));
            sbox[p] = (uint8_t)(x ^ 0x63);
        } while (p != 1);
        sbox[0] = 0x63;

        for (int i = 0; i < 256; i++) {
            uint8_t s = sbox[i];
            uint8_t s2 = XTime(s);
            uint8_t s3 = (uint8_t)(s2 ^ s);
            uint32_t word = ((uint32_t)s2 << 24) | ((uint32_t)s << 16) | ((uint32_t)s << 8) | (uint32_t)s3;
            te[0][i] = word;
            te[1][i] = (word >> 8) | (word << 24);
            te[2][i] = (word >> 16) | (word << 16);
            te[3][i] = (word >> 24) | (word << 8);
        }
    }

    static uint8_t RotL8(uint8_t x, int n) { return (uint8_t)((x << n) | (x >> (8 - n))); }
    static uint8_t XTime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1B : 0)); }
};

const Tables& GetTables() {
    static const Tables tables;
    return tables;
}

uint32_t LoadBE(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void StoreBE(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

uint32_t SubWord(const Tables& t, uint32_t w) {
    return ((uint32_t)t.sbox[w >> 24] << 24) | ((uint32_t)t.sbox[(w >> 16) & 0xff] << 16) |
           ((uint32_t)t.sbox[(w >> 8) & 0xff] << 8) | (uint32_t)t.sbox[w & 0xff];
}

}

void Aes256::ExpandKey(const uint8_t key[32], Key& out) {
    const Tables& t = GetTables();
    uint32_t* w = out.roundKeys;
    for (int i = 0; i < 8; i++) w[i] = LoadBE(key + i * 4);

    uint8_t rcon = 1;
    for (int i = 8; i < 60; i++) {
        uint32_t temp = w[i - 1];
        if (i % 8 == 0) {
            temp = (temp << 8) | (temp >> 24);
            temp = SubWord(t, temp) ^ ((uint32_t)rcon << 24);
            rcon = (uint8_t)((rcon << 1) ^ ((rcon & 0x80) ? 0x1B : 0));
        } else if (i % 8 == 4) {
            temp = SubWord(t, temp);
        }
        w[i] = w[i - 8] ^ temp;
    }
}

void Aes256::EncryptBlock(const Key& key, const uint8_t in[16], uint8_t out[16]) {
    const Tables& t = GetTables();
    const uint32_t* rk = key.roundKeys;

    uint32_t s0 = LoadBE(in) ^ rk[0];
    uint32_t s1 = LoadBE(in + 4) ^ rk[1];
    uint32_t s2 = LoadBE(in + 8) ^ rk[2];
    uint32_t s3 = LoadBE(in + 12) ^ rk[3];

    for (int round = 1; round < 14; round++) {
        rk += 4;
        uint32_t t0 = t.te[0][s0 >> 24] ^ t.te[1][(s1 >> 16) & 0xff] ^ t.te[2][(s2 >> 8) & 0xff] ^ t.te[3][s3 & 0xff] ^ rk[0];
        uint32_t t1 = t.te[0][s1 >> 24] ^ t.te[1][(s2 >> 16) & 0xff] ^ t.te[2][(s3 >> 8) & 0xff] ^ t.te[3][s0 & 0xff] ^ rk[1];
        uint32_t t2 = t.te[0][s2 >> 24] ^ t.te[1][(s3 >> 16) & 0xff] ^ t.te[2][(s0 >> 8) & 0xff] ^ t.te[3][s1 & 0xff] ^ rk[2];
        uint32_t t3 = t.te[0][s3 >> 24] ^ t.te[1][(s0 >> 16) & 0xff] ^ t.te[2][(s1 >> 8) & 0xff] ^ t.te[3][s2 & 0xff] ^ rk[3];
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }

    rk += 4;
    uint32_t o0 = (((uint32_t)t.sbox[s0 >> 24] << 24) | ((uint32_t)t.sbox[(s1 >> 16) & 0xff] << 16) |
                   ((uint32_t)t.sbox[(s2 >> 8) & 0xff] << 8) | (uint32_t)t.sbox[s3 & 0xff]) ^ rk[0];
    uint32_t o1 = (((uint32_t)t.sbox[s1 >> 24] << 24) | ((uint32_t)t.sbox[(s2 >> 16) & 0xff] << 16) |
                   ((uint32_t)t.sbox[(s3 >> 8) & 0xff] << 8) | (uint32_t)t.sbox[s0 & 0xff]) ^ rk[1];
    uint32_t o2 = (((uint32_t)t.sbox[s2 >> 24] << 24) | ((uint32_t)t.sbox[(s3 >> 16) & 0xff] << 16) |
                   ((uint32_t)t.sbox[(s0 >> 8) & 0xff] << 8) | (uint32_t)t.sbox[s1 & 0xff]) ^ rk[2];
    uint32_t o3 = (((uint32_t)t.sbox[s3 >> 24] << 24) | ((uint32_t)t.sbox[(s0 >> 16) & 0xff] << 16) |
                   ((uint32_t)t.sbox[(s1 >> 8) & 0xff] << 8) | (uint32_t)t.sbox[s2 & 0xff]) ^ rk[3];
    StoreBE(out, o0);
    StoreBE(out + 4, o1);
    StoreBE(out + 8, o2);
    StoreBE(out + 12, o3);
}

void Aes256::CtrXor(const Key& key, const uint8_t counter[16], uint8_t* data, size_t size) {
    uint8_t block[16];
    std::memcpy(block, counter, 16);
    uint8_t stream[16];

    size_t offset = 0;
    while (offset < size) {
        EncryptBlock(key, block, stream);
        size_t chunk = size - offset < 16 ? size - offset : 16;
        for (size_t i = 0; i < chunk; i++) data[offset + i] = (uint8_t)(data[offset + i] ^ stream[i]);
        offset += chunk;
        for (int i = 15; i >= 0; i--) {
            if (++block[i] != 0) break;
        }
    }
}
