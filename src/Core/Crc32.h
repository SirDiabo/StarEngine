#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Crc32 {

struct Tables {
    uint32_t t[8][256];
};

inline const Tables& GetTables() {
    static const Tables tables = []() {
        Tables out{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            out.t[0][i] = c;
        }
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = out.t[0][i];
            for (int s = 1; s < 8; s++) {
                c = out.t[0][c & 0xFFu] ^ (c >> 8);
                out.t[s][i] = c;
            }
        }
        return out;
    }();
    return tables;
}

inline uint32_t Update(uint32_t crc, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    const Tables& tb = GetTables();
    uint32_t c = ~crc;
    while (size >= 8) {
        const uint32_t a = c ^ ((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
        c = tb.t[7][a & 0xFFu] ^ tb.t[6][(a >> 8) & 0xFFu] ^ tb.t[5][(a >> 16) & 0xFFu] ^ tb.t[4][a >> 24] ^
            tb.t[3][p[4]] ^ tb.t[2][p[5]] ^ tb.t[1][p[6]] ^ tb.t[0][p[7]];
        p += 8;
        size -= 8;
    }
    while (size > 0) {
        c = tb.t[0][(c ^ *p) & 0xFFu] ^ (c >> 8);
        p++;
        size--;
    }
    return ~c;
}

inline uint32_t Compute(const void* data, size_t size) {
    return Update(0, data, size);
}

inline uint32_t Compute(const std::vector<uint8_t>& data) {
    return Update(0, data.data(), data.size());
}

inline uint32_t Compute(const std::string& text) {
    return Update(0, text.data(), text.size());
}

inline std::string ToHex(uint32_t value) {
    static const char digits[] = "0123456789abcdef";
    std::string out(8, '0');
    for (int i = 7; i >= 0; i--) {
        out[(size_t)i] = digits[value & 15u];
        value >>= 4;
    }
    return out;
}

inline bool FromHex(const std::string& text, uint32_t& out) {
    if (text.size() != 8) return false;
    uint32_t value = 0;
    for (char c : text) {
        uint32_t nibble;
        if (c >= '0' && c <= '9') nibble = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') nibble = (uint32_t)(c - 'A' + 10);
        else return false;
        value = (value << 4) | nibble;
    }
    out = value;
    return true;
}

}
