#include "CookedText.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <unordered_map>

namespace {

constexpr char kMagic[5] = {'S', 'T', 'X', 'T', '1'};
constexpr uint32_t kLiteralCodes = 256;

class Mixer {
public:
    explicit Mixer(uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}

    uint32_t Next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    uint8_t Byte() { return (uint8_t)(Next() >> 11); }

private:
    uint32_t state;
};

bool IsWordByte(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

void PutVarint(std::vector<uint8_t>& out, uint32_t value) {
    while (value >= 0x80) {
        out.push_back((uint8_t)(value | 0x80));
        value >>= 7;
    }
    out.push_back((uint8_t)value);
}

bool GetVarint(const uint8_t* data, size_t size, size_t& pos, uint32_t& value) {
    value = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        if (pos >= size) return false;
        const uint8_t byte = data[pos++];
        if (shift == 28 && (byte & 0xF0) != 0) return false;
        value |= (uint32_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

void BuildPermutation(uint32_t seed, uint8_t forward[256], uint8_t inverse[256]) {
    Mixer mixer(seed ^ 0xA5A5A5A5u);
    for (int i = 0; i < 256; i++) forward[i] = (uint8_t)i;
    for (int i = 255; i > 0; i--) std::swap(forward[i], forward[mixer.Next() % (uint32_t)(i + 1)]);
    for (int i = 0; i < 256; i++) inverse[forward[i]] = (uint8_t)i;
}

}

bool CookedText::LooksCooked(const uint8_t* data, size_t size) {
    return data != nullptr && size >= sizeof(kMagic) + 4 && std::memcmp(data, kMagic, sizeof(kMagic)) == 0;
}

bool CookedText::Pack(const std::string& text, uint32_t seed, std::vector<uint8_t>& out) try {
    out.clear();
    if (text.size() > kMaxTextBytes) return false;

    uint8_t forward[256];
    uint8_t inverse[256];
    BuildPermutation(seed, forward, inverse);

    std::vector<std::string> words;
    std::unordered_map<std::string, uint32_t> wordIndex;
    std::vector<uint32_t> codes;
    codes.reserve(text.size() / 2 + 1);

    size_t i = 0;
    while (i < text.size()) {
        const uint8_t c = (uint8_t)text[i];
        if (!IsWordByte(c)) {
            codes.push_back(forward[c]);
            i++;
            continue;
        }
        size_t end = i;
        while (end < text.size() && IsWordByte((uint8_t)text[end])) end++;
        std::string word = text.substr(i, end - i);
        auto found = wordIndex.find(word);
        uint32_t index;
        if (found == wordIndex.end()) {
            if (words.size() >= kMaxDictionaryWords) return false;
            index = (uint32_t)words.size();
            wordIndex.emplace(word, index);
            words.push_back(std::move(word));
        } else {
            index = found->second;
        }
        codes.push_back(kLiteralCodes + index);
        i = end;
    }

    out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
    for (int b = 0; b < 4; b++) out.push_back((uint8_t)(seed >> (8 * b)));
    PutVarint(out, (uint32_t)text.size());
    PutVarint(out, (uint32_t)words.size());

    Mixer mask(seed);
    for (const std::string& word : words) {
        PutVarint(out, (uint32_t)word.size());
        for (char c : word) out.push_back((uint8_t)((uint8_t)c ^ mask.Byte()));
    }
    for (uint32_t code : codes) PutVarint(out, code);
    return true;
} catch (const std::exception&) {
    out.clear();
    return false;
}

bool CookedText::Unpack(const uint8_t* data, size_t size, std::string& out) try {
    out.clear();
    if (!LooksCooked(data, size)) return false;

    size_t pos = sizeof(kMagic);
    uint32_t seed = 0;
    for (int b = 0; b < 4; b++) seed |= (uint32_t)data[pos++] << (8 * b);

    uint32_t originalSize = 0;
    uint32_t wordCount = 0;
    if (!GetVarint(data, size, pos, originalSize) || !GetVarint(data, size, pos, wordCount)) return false;
    if (originalSize > kMaxTextBytes || wordCount > kMaxDictionaryWords) return false;
    if (wordCount > size - pos) return false;

    uint8_t forward[256];
    uint8_t inverse[256];
    BuildPermutation(seed, forward, inverse);

    std::vector<std::string> words;
    words.reserve(wordCount);
    Mixer mask(seed);
    for (uint32_t w = 0; w < wordCount; w++) {
        uint32_t length = 0;
        if (!GetVarint(data, size, pos, length)) return false;
        if (length == 0 || length > originalSize || length > size - pos) return false;
        std::string word(length, '\0');
        for (uint32_t n = 0; n < length; n++) word[n] = (char)(data[pos + n] ^ mask.Byte());
        pos += length;
        words.push_back(std::move(word));
    }

    std::string result;
    result.reserve(std::min<size_t>(originalSize, size * 32));
    while (pos < size) {
        uint32_t code = 0;
        if (!GetVarint(data, size, pos, code)) return false;
        if (code < kLiteralCodes) {
            if (result.size() + 1 > originalSize) return false;
            result.push_back((char)inverse[code]);
        } else {
            const uint32_t index = code - kLiteralCodes;
            if (index >= words.size()) return false;
            const std::string& word = words[index];
            if (word.size() > originalSize - result.size()) return false;
            result += word;
        }
    }
    if (result.size() != originalSize) return false;
    out = std::move(result);
    return true;
} catch (const std::exception&) {
    out.clear();
    return false;
}
