#include "BakedKey.h"
#include "Crc32.h"
#include "Sha256.h"
#include <algorithm>
#include <cstdio>

#if defined(__has_include)
#if __has_include("PackageKey.generated.h")
#include "PackageKey.generated.h"
#define STARENGINE_HAS_BAKED_KEY 1
#endif
#endif

namespace {

class Drbg {
public:
    explicit Drbg(const std::vector<uint8_t>& seedBytes) : seed(seedBytes) {}

    uint8_t Byte() {
        if (position >= 32) Refill();
        return block.bytes[position++];
    }

    uint32_t Below(uint32_t limit) {
        if (limit <= 1) return 0;
        const uint32_t bound = 0xFFFFFFFFu - (0xFFFFFFFFu % limit);
        for (;;) {
            uint32_t value = (uint32_t)Byte() | ((uint32_t)Byte() << 8) | ((uint32_t)Byte() << 16) | ((uint32_t)Byte() << 24);
            if (value < bound) return value % limit;
        }
    }

private:
    void Refill() {
        std::vector<uint8_t> material = seed;
        for (int i = 0; i < 8; i++) material.push_back((uint8_t)(counter >> (8 * i)));
        counter++;
        block = Sha256::Compute(material);
        position = 0;
    }

    std::vector<uint8_t> seed;
    uint64_t counter = 0;
    Sha256::Digest block{};
    size_t position = 32;
};

constexpr size_t kHeaderBytes = 4;
constexpr size_t kTableEntryBytes = 8;
constexpr size_t kMaxParts = 16;
constexpr size_t kMaxPayloadBytes = BakedKey::kMaxCipherName + BakedKey::kMaxKeyBytes + 16;

uint8_t TableMask(uint8_t salt, size_t index) {
    return (uint8_t)(salt + 31u * (uint32_t)index + 7u);
}

uint8_t PartMask(uint8_t seed, uint8_t step, size_t index) {
    return (uint8_t)(seed + (uint32_t)step * (uint32_t)index);
}

bool BuildPayload(const BakedKey::Material& material, std::vector<uint8_t>& payload) {
    if (material.cipher.empty() || material.cipher.size() > BakedKey::kMaxCipherName) return false;
    if (material.key.empty() || material.key.size() > BakedKey::kMaxKeyBytes) return false;

    payload.clear();
    payload.push_back((uint8_t)material.cipher.size());
    payload.insert(payload.end(), material.cipher.begin(), material.cipher.end());
    payload.push_back((uint8_t)(material.key.size() & 0xFF));
    payload.push_back((uint8_t)(material.key.size() >> 8));
    payload.insert(payload.end(), material.key.begin(), material.key.end());

    const uint32_t crc = Crc32::Compute(payload);
    for (int i = 0; i < 4; i++) payload.push_back((uint8_t)(crc >> (8 * i)));
    return true;
}

bool ParsePayload(const std::vector<uint8_t>& payload, BakedKey::Material& out) {
    if (payload.size() < 1 + 1 + 2 + 1 + 4) return false;
    const size_t bodySize = payload.size() - 4;
    uint32_t stored = 0;
    for (int i = 0; i < 4; i++) stored |= (uint32_t)payload[bodySize + (size_t)i] << (8 * i);
    if (Crc32::Compute(payload.data(), bodySize) != stored) return false;

    size_t pos = 0;
    const size_t cipherLength = payload[pos++];
    if (cipherLength == 0 || cipherLength > BakedKey::kMaxCipherName || pos + cipherLength + 2 > bodySize) return false;
    std::string cipher(reinterpret_cast<const char*>(payload.data()) + pos, cipherLength);
    pos += cipherLength;

    const size_t keyLength = (size_t)payload[pos] | ((size_t)payload[pos + 1] << 8);
    pos += 2;
    if (keyLength == 0 || keyLength > BakedKey::kMaxKeyBytes || pos + keyLength != bodySize) return false;
    std::string key(reinterpret_cast<const char*>(payload.data()) + pos, keyLength);

    out.cipher = std::move(cipher);
    out.key = std::move(key);
    return true;
}

}

bool BakedKey::Encode(const Material& material, const std::vector<uint8_t>& entropy, std::vector<uint8_t>& blob) {
    blob.clear();
    if (entropy.size() < kMinEntropyBytes) return false;

    std::vector<uint8_t> payload;
    if (!BuildPayload(material, payload)) return false;

    Drbg rng(entropy);

    uint8_t salt = rng.Byte();
    if (salt == 0) salt = 0x5B;

    size_t partCount = 3 + rng.Below(5);
    if (partCount > payload.size()) partCount = payload.size();

    std::vector<size_t> cuts;
    while (cuts.size() + 1 < partCount) {
        size_t cut = 1 + rng.Below((uint32_t)(payload.size() - 1));
        if (std::find(cuts.begin(), cuts.end(), cut) == cuts.end()) cuts.push_back(cut);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.push_back(payload.size());

    struct Part {
        size_t order = 0;
        std::vector<uint8_t> bytes;
        uint8_t seed = 0;
        uint8_t step = 0;
        size_t offset = 0;
    };
    std::vector<Part> parts;
    size_t from = 0;
    for (size_t i = 0; i < cuts.size(); i++) {
        Part part;
        part.order = i;
        part.seed = rng.Byte();
        part.step = rng.Byte();
        for (size_t n = 0; n < cuts[i] - from; n++) {
            part.bytes.push_back((uint8_t)(payload[from + n] ^ PartMask(part.seed, part.step, n)));
        }
        from = cuts[i];
        parts.push_back(std::move(part));
    }

    std::vector<size_t> placement(parts.size());
    for (size_t i = 0; i < placement.size(); i++) placement[i] = i;
    for (size_t i = placement.size(); i > 1; i--) std::swap(placement[i - 1], placement[rng.Below((uint32_t)i)]);

    auto appendDecoy = [&]() {
        const size_t count = rng.Below(41);
        for (size_t i = 0; i < count; i++) blob.push_back(rng.Byte());
    };

    blob.assign(kHeaderBytes, 0);
    const size_t tableSlot = rng.Below((uint32_t)parts.size() + 1);
    size_t tableOffset = 0;
    for (size_t slot = 0; slot <= parts.size(); slot++) {
        appendDecoy();
        if (slot == tableSlot) {
            tableOffset = blob.size();
            blob.insert(blob.end(), parts.size() * kTableEntryBytes, 0);
        }
        if (slot < parts.size()) {
            Part& part = parts[placement[slot]];
            part.offset = blob.size();
            blob.insert(blob.end(), part.bytes.begin(), part.bytes.end());
        }
    }
    appendDecoy();

    if (blob.size() > 0xFFFF) {
        blob.clear();
        return false;
    }

    for (size_t i = 0; i < parts.size(); i++) {
        const Part& part = parts[i];
        uint8_t entry[kTableEntryBytes];
        entry[0] = (uint8_t)(part.offset & 0xFF);
        entry[1] = (uint8_t)(part.offset >> 8);
        entry[2] = (uint8_t)(part.bytes.size() & 0xFF);
        entry[3] = (uint8_t)(part.bytes.size() >> 8);
        entry[4] = (uint8_t)part.order;
        entry[5] = part.seed;
        entry[6] = part.step;
        entry[7] = rng.Byte();
        for (size_t j = 0; j < kTableEntryBytes; j++) {
            blob[tableOffset + i * kTableEntryBytes + j] = (uint8_t)(entry[j] ^ TableMask(salt, i * kTableEntryBytes + j));
        }
    }

    blob[0] = salt;
    blob[1] = (uint8_t)((tableOffset & 0xFF) ^ salt);
    blob[2] = (uint8_t)((tableOffset >> 8) ^ salt);
    blob[3] = (uint8_t)(parts.size() ^ salt);
    return true;
}

bool BakedKey::Decode(const uint8_t* blob, size_t size, Material& out) {
    if (blob == nullptr || size < kHeaderBytes || size > 0xFFFF) return false;

    const uint8_t salt = blob[0];
    const size_t tableOffset = (size_t)(blob[1] ^ salt) | ((size_t)(blob[2] ^ salt) << 8);
    const size_t partCount = (size_t)(blob[3] ^ salt);
    if (partCount == 0 || partCount > kMaxParts) return false;
    if (tableOffset < kHeaderBytes || tableOffset > size || partCount * kTableEntryBytes > size - tableOffset) return false;

    struct Slot {
        bool used = false;
        size_t offset = 0;
        size_t length = 0;
        uint8_t seed = 0;
        uint8_t step = 0;
    };
    std::vector<Slot> slots(partCount);
    size_t total = 0;
    for (size_t i = 0; i < partCount; i++) {
        uint8_t entry[kTableEntryBytes];
        for (size_t j = 0; j < kTableEntryBytes; j++) {
            entry[j] = (uint8_t)(blob[tableOffset + i * kTableEntryBytes + j] ^ TableMask(salt, i * kTableEntryBytes + j));
        }
        const size_t offset = (size_t)entry[0] | ((size_t)entry[1] << 8);
        const size_t length = (size_t)entry[2] | ((size_t)entry[3] << 8);
        const size_t order = entry[4];
        if (order >= partCount || slots[order].used) return false;
        if (length == 0 || offset < kHeaderBytes || offset > size || length > size - offset) return false;
        if (total > kMaxPayloadBytes - length) return false;
        total += length;
        slots[order].used = true;
        slots[order].offset = offset;
        slots[order].length = length;
        slots[order].seed = entry[5];
        slots[order].step = entry[6];
    }

    std::vector<uint8_t> payload;
    payload.reserve(total);
    for (const Slot& slot : slots) {
        for (size_t n = 0; n < slot.length; n++) {
            payload.push_back((uint8_t)(blob[slot.offset + n] ^ PartMask(slot.seed, slot.step, n)));
        }
    }
    return ParsePayload(payload, out);
}

std::string BakedKey::BuildHeaderText(const std::vector<uint8_t>& blob) {
    std::string out = "#pragma once\n\nnamespace BakedKeyData {\n\ninline constexpr unsigned char kBlob[] = {\n";
    for (size_t i = 0; i < blob.size(); i++) {
        char text[8];
        std::snprintf(text, sizeof(text), "0x%02x", (unsigned)blob[i]);
        out += (i % 16 == 0) ? "    " : " ";
        out += text;
        if (i + 1 < blob.size()) out += ",";
        if (i % 16 == 15 || i + 1 == blob.size()) out += "\n";
    }
    out += "};\n\ninline constexpr unsigned long kBlobSize = sizeof(kBlob);\n\n}\n";
    return out;
}

std::string BakedKey::BuildEmptyHeaderText() {
    return "#pragma once\n\nnamespace BakedKeyData {\n\ninline constexpr unsigned char kBlob[] = {0};\n\ninline constexpr unsigned long kBlobSize = 0;\n\n}\n";
}

bool BakedKey::Get(Material& out) {
#ifdef STARENGINE_HAS_BAKED_KEY
    static bool decoded = false;
    static bool valid = false;
    static Material cached;
    if (!decoded) {
        decoded = true;
        valid = Decode(BakedKeyData::kBlob, (size_t)BakedKeyData::kBlobSize, cached);
    }
    if (!valid) return false;
    out = cached;
    return true;
#else
    (void)out;
    return false;
#endif
}
