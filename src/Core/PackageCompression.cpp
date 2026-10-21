#include "PackageCompression.h"
#include "Module.h"
#include "raylib.h"
#include <climits>
#include <cstring>
#include <memory>

namespace {

constexpr int kFastBits = 10;
constexpr uint64_t kMaxDeflateRatio = 1032;

struct BitReader {
    const uint8_t* p;
    const uint8_t* end;
    uint64_t buf = 0;
    int cnt = 0;
    bool error = false;

    void Refill() {
        while (cnt <= 56 && p < end) {
            buf |= (uint64_t)(*p++) << cnt;
            cnt += 8;
        }
    }

    uint32_t Get(int n) {
        if (n == 0) return 0;
        if (cnt < n) {
            Refill();
            if (cnt < n) {
                error = true;
                return 0;
            }
        }
        uint32_t value = (uint32_t)(buf & ((1ull << n) - 1));
        buf >>= n;
        cnt -= n;
        return value;
    }
};

struct Huffman {
    uint16_t fast[1 << kFastBits];
    uint16_t count[16];
    uint16_t symbol[320];
};

uint32_t ReverseBits(uint32_t code, int len) {
    uint32_t result = 0;
    for (int i = 0; i < len; i++) {
        result = (result << 1) | (code & 1u);
        code >>= 1;
    }
    return result;
}

bool BuildHuffman(Huffman& h, const uint8_t* lengths, int n) {
    std::memset(&h, 0, sizeof(h));
    for (int i = 0; i < n; i++) {
        if (lengths[i] > 15) return false;
        h.count[lengths[i]]++;
    }
    h.count[0] = 0;

    int left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return false;
    }

    uint16_t start[16];
    start[1] = 0;
    for (int len = 1; len < 15; len++) start[len + 1] = (uint16_t)(start[len] + h.count[len]);

    uint16_t cursor[16];
    std::memcpy(cursor, start, sizeof(cursor));
    for (int i = 0; i < n; i++) {
        if (lengths[i]) h.symbol[cursor[lengths[i]]++] = (uint16_t)i;
    }

    uint32_t code = 0;
    for (int len = 1; len <= 15; len++) {
        code = (code + (len > 1 ? h.count[len - 1] : 0u)) << 1;
        if (len > kFastBits) continue;
        for (int k = 0; k < h.count[len]; k++) {
            uint32_t symbol = h.symbol[start[len] + k];
            uint32_t reversed = ReverseBits(code + (uint32_t)k, len);
            uint16_t entry = (uint16_t)((symbol << 4) | (uint32_t)len);
            for (uint32_t fill = reversed; fill < (1u << kFastBits); fill += (1u << len)) h.fast[fill] = entry;
        }
    }
    return true;
}

int DecodeSymbol(BitReader& br, const Huffman& h) {
    if (br.cnt < 15) br.Refill();
    uint16_t entry = h.fast[br.buf & ((1u << kFastBits) - 1)];
    if (entry) {
        int len = entry & 15;
        if (len > br.cnt) {
            br.error = true;
            return -1;
        }
        br.buf >>= len;
        br.cnt -= len;
        return entry >> 4;
    }

    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= 15; len++) {
        if (br.cnt < len) {
            br.error = true;
            return -1;
        }
        code |= (int)((br.buf >> (len - 1)) & 1u);
        int c = h.count[len];
        if (code - c < first) {
            br.buf >>= len;
            br.cnt -= len;
            return h.symbol[index + (code - first)];
        }
        index += c;
        first += c;
        first <<= 1;
        code <<= 1;
    }
    br.error = true;
    return -1;
}

const uint16_t kLengthBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uint8_t kLengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const uint16_t kDistBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
const uint8_t kDistExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
const uint8_t kCodeLengthOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

struct FixedTables {
    Huffman lit;
    Huffman dist;
    FixedTables() {
        uint8_t lengths[288];
        for (int i = 0; i < 144; i++) lengths[i] = 8;
        for (int i = 144; i < 256; i++) lengths[i] = 9;
        for (int i = 256; i < 280; i++) lengths[i] = 7;
        for (int i = 280; i < 288; i++) lengths[i] = 8;
        BuildHuffman(lit, lengths, 288);
        uint8_t distLengths[32];
        for (int i = 0; i < 32; i++) distLengths[i] = 5;
        BuildHuffman(dist, distLengths, 32);
    }
};

bool InflateBlock(BitReader& br, const Huffman& lit, const Huffman& dist, uint8_t* out, size_t cap, size_t& pos) {
    while (true) {
        int sym = DecodeSymbol(br, lit);
        if (sym < 0) return false;
        if (sym < 256) {
            if (pos >= cap) return false;
            out[pos++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256) return true;

        sym -= 257;
        if (sym >= 29) return false;
        size_t len = kLengthBase[sym] + br.Get(kLengthExtra[sym]);
        int distSym = DecodeSymbol(br, dist);
        if (distSym < 0 || distSym >= 30) return false;
        size_t offs = kDistBase[distSym] + br.Get(kDistExtra[distSym]);
        if (br.error) return false;
        if (offs > pos || len > cap - pos) return false;

        uint8_t* dst = out + pos;
        const uint8_t* src = dst - offs;
        if (offs >= len) {
            std::memcpy(dst, src, len);
        } else if (offs == 1) {
            std::memset(dst, *src, len);
        } else {
            for (size_t i = 0; i < len; i++) dst[i] = src[i];
        }
        pos += len;
    }
}

bool InflateDynamicHeader(BitReader& br, Huffman& lit, Huffman& dist) {
    int nlen = (int)br.Get(5) + 257;
    int ndist = (int)br.Get(5) + 1;
    int ncode = (int)br.Get(4) + 4;
    if (br.error || nlen > 286 || ndist > 30) return false;

    uint8_t codeLengths[19] = {};
    for (int i = 0; i < ncode; i++) codeLengths[kCodeLengthOrder[i]] = (uint8_t)br.Get(3);
    if (br.error) return false;

    Huffman lengthCode;
    if (!BuildHuffman(lengthCode, codeLengths, 19)) return false;

    uint8_t all[320] = {};
    int total = nlen + ndist;
    int index = 0;
    while (index < total) {
        int sym = DecodeSymbol(br, lengthCode);
        if (sym < 0) return false;
        if (sym < 16) {
            all[index++] = (uint8_t)sym;
            continue;
        }
        int repeat = 0;
        uint8_t value = 0;
        if (sym == 16) {
            if (index == 0) return false;
            value = all[index - 1];
            repeat = 3 + (int)br.Get(2);
        } else if (sym == 17) {
            repeat = 3 + (int)br.Get(3);
        } else {
            repeat = 11 + (int)br.Get(7);
        }
        if (br.error || index + repeat > total) return false;
        while (repeat--) all[index++] = value;
    }
    if (all[256] == 0) return false;
    if (!BuildHuffman(lit, all, nlen)) return false;
    return BuildHuffman(dist, all + nlen, ndist);
}

bool Inflate(const uint8_t* in, size_t inSize, uint8_t* out, size_t cap) {
    static const FixedTables fixedTables;
    BitReader br{ in, in + inSize };
    size_t pos = 0;
    bool last = false;
    std::unique_ptr<Huffman[]> dynamicTables(new Huffman[2]);

    while (!last) {
        last = br.Get(1) != 0;
        uint32_t type = br.Get(2);
        if (br.error) return false;

        if (type == 0) {
            int discard = br.cnt & 7;
            br.buf >>= discard;
            br.cnt -= discard;
            uint32_t len = br.Get(16);
            uint32_t nlen = br.Get(16);
            if (br.error || (len ^ 0xFFFFu) != nlen || len > cap - pos) return false;
            for (uint32_t i = 0; i < len; i++) out[pos++] = (uint8_t)br.Get(8);
            if (br.error) return false;
        } else if (type == 1) {
            if (!InflateBlock(br, fixedTables.lit, fixedTables.dist, out, cap, pos)) return false;
        } else if (type == 2) {
            if (!InflateDynamicHeader(br, dynamicTables[0], dynamicTables[1])) return false;
            if (!InflateBlock(br, dynamicTables[0], dynamicTables[1], out, cap, pos)) return false;
        } else {
            return false;
        }
    }
    return pos == cap;
}

bool DeflateCompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    if (input.empty() || input.size() > (size_t)INT_MAX / 2) return false;

    int compressedSize = 0;
    unsigned char* packed = CompressData(input.data(), (int)input.size(), &compressedSize);
    if (!packed) return false;

    bool ok = compressedSize > 0;
    if (ok) output.assign(packed, packed + compressedSize);
    MemFree(packed);
    return ok;
}

bool DeflateDecompress(const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output) {
    if (input.empty() || rawSize == 0) return false;
    if (rawSize > (uint64_t)input.size() * kMaxDeflateRatio) return false;

    output.assign((size_t)rawSize, 0);
    if (!Inflate(input.data(), input.size(), output.data(), output.size())) {
        output.clear();
        return false;
    }
    return true;
}


class DeflateCompressor : public IPackCompressor {
public:
    bool Compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const override {
        return DeflateCompress(input, output);
    }

    bool Decompress(const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output) const override {
        return DeflateDecompress(input, rawSize, output);
    }
};

}

PackCompressionRegistry& PackCompressionRegistry::Get() {
    static PackCompressionRegistry instance;
    return instance;
}

PackCompressionRegistry::PackCompressionRegistry() {
    PackCompressionInfo none;
    none.id = PackCompressions::None;
    none.displayName = "None";
    none.description = "Files are stored as they are.";
    methods.push_back(none);

    PackCompressionInfo deflate;
    deflate.id = PackCompressions::Deflate;
    deflate.displayName = "Deflate (smaller package)";
    deflate.description = "Applied before encryption. Files that do not shrink, such as png or ogg, are stored as they are.";
    deflate.compressor = std::make_shared<DeflateCompressor>();
    methods.push_back(deflate);
}

void PackCompressionRegistry::Register(const PackCompressionInfo& info) {
    if (info.id.empty() || !info.compressor) return;
    std::lock_guard<std::mutex> lock(mutex);
    for (PackCompressionInfo& existing : methods) {
        if (existing.id == info.id) {
            existing = info;
            return;
        }
    }
    methods.push_back(info);
}

bool PackCompressionRegistry::Find(const std::string& id, PackCompressionInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex);
    for (const PackCompressionInfo& info : methods) {
        if (info.id == id) {
            out = info;
            return true;
        }
    }
    return false;
}

bool PackCompressionRegistry::IsAvailable(const PackCompressionInfo& info) const {
    return info.moduleName.empty() || ModuleManager::Get().IsModuleEnabled(info.moduleName);
}

std::vector<PackCompressionInfo> PackCompressionRegistry::GetAll() const {
    std::lock_guard<std::mutex> lock(mutex);
    return methods;
}

bool PackageCompression::Compress(const std::string& id, const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.clear();
    PackCompressionInfo info;
    if (!PackCompressionRegistry::Get().Find(id, info) || !info.compressor) return false;
    return info.compressor->Compress(input, output);
}

bool PackageCompression::Decompress(const std::string& id, const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output) {
    output.clear();
    if (id.empty() || id == PackCompressions::None) {
        if (input.size() != rawSize) return false;
        output = input;
        return true;
    }
    PackCompressionInfo info;
    if (!PackCompressionRegistry::Get().Find(id, info) || !info.compressor) return false;
    return info.compressor->Decompress(input, rawSize, output);
}
