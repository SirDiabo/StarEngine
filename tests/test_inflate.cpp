#include "TestHelper.h"
#include "Core/PackageCompression.h"
#include "Core/Sha256.h"
#include "data/InflateVectors.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace {

std::atomic<size_t> g_largestRequest{ 0 };
std::atomic<bool> g_trackRequests{ false };
constexpr size_t kRefuseAbove = 64u * 1024u * 1024u;

}

void* operator new(size_t size) {
    if (g_trackRequests.load(std::memory_order_relaxed)) {
        size_t seen = g_largestRequest.load();
        while (size > seen && !g_largestRequest.compare_exchange_weak(seen, size)) {
        }
        if (size > kRefuseAbove) throw std::bad_alloc();
    }
    void* p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

namespace {

class DeflateBits {
public:
    void Lsb(uint32_t value, int count) {
        for (int i = 0; i < count; i++) Push((value >> i) & 1u);
    }
    void Msb(uint32_t code, int count) {
        for (int i = count - 1; i >= 0; i--) Push((code >> i) & 1u);
    }
    void Align() {
        while (bitCount % 8) Push(0);
    }
    void Byte(uint8_t value) {
        Align();
        bytes.push_back(value);
        bitCount += 8;
    }
    std::vector<uint8_t> Finish() {
        Align();
        return bytes;
    }

private:
    void Push(uint32_t bit) {
        if (bitCount % 8 == 0) bytes.push_back(0);
        if (bit) bytes.back() = (uint8_t)(bytes.back() | (1u << (bitCount % 8)));
        bitCount++;
    }
    std::vector<uint8_t> bytes;
    size_t bitCount = 0;
};

void FixedHeader(DeflateBits& bits, bool final) {
    bits.Lsb(final ? 1 : 0, 1);
    bits.Lsb(1, 2);
}

void FixedSymbol(DeflateBits& bits, uint32_t symbol) {
    if (symbol < 144) bits.Msb(0x30 + symbol, 8);
    else if (symbol < 256) bits.Msb(0x190 + (symbol - 144), 9);
    else if (symbol < 280) bits.Msb(symbol - 256, 7);
    else bits.Msb(0xC0 + (symbol - 280), 8);
}

void FixedDistance(DeflateBits& bits, uint32_t symbol) {
    bits.Msb(symbol, 5);
}

std::vector<uint8_t> StoredBlock(const std::vector<uint8_t>& payload, bool final, uint16_t lenOverride = 0, bool overrideLen = false, bool breakComplement = false) {
    DeflateBits bits;
    bits.Lsb(final ? 1 : 0, 1);
    bits.Lsb(0, 2);
    uint16_t len = overrideLen ? lenOverride : (uint16_t)payload.size();
    uint16_t nlen = (uint16_t)~len;
    if (breakComplement) nlen = (uint16_t)(nlen ^ 1);
    bits.Byte((uint8_t)(len & 0xFF));
    bits.Byte((uint8_t)(len >> 8));
    bits.Byte((uint8_t)(nlen & 0xFF));
    bits.Byte((uint8_t)(nlen >> 8));
    for (uint8_t b : payload) bits.Byte(b);
    return bits.Finish();
}

bool InflateOk(const std::vector<uint8_t>& stream, uint64_t rawSize, std::vector<uint8_t>& out) {
    return PackageCompression::Decompress("Deflate", stream, rawSize, out);
}

const char* kDeflate = "Deflate";
const char* kNone = "None";

std::vector<uint8_t> VectorBytes(const vectors::InflateVector& v) {
    return std::vector<uint8_t>(v.data, v.data + v.size);
}

std::string DigestHex(const std::vector<uint8_t>& data) {
    return Sha256::ToHex(Sha256::Compute(data));
}

const vectors::InflateVector* FindVector(const char* name) {
    for (const vectors::InflateVector& v : vectors::kInflate) {
        if (std::string(v.name) == name) return &v;
    }
    return nullptr;
}

std::vector<uint8_t> TextLike(size_t count, uint64_t seed) {
    static const char* words[] = { "star", "engine", "actor", "level", "shadow", "light", "module", "graph", "script", "mesh" };
    test::Rng rng(seed);
    std::string text;
    while (text.size() < count) {
        text += words[rng.Below(10)];
        text += rng.Below(7) ? ' ' : '\n';
    }
    text.resize(count);
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::vector<uint8_t> FarCopy(size_t blockSize, uint64_t seed) {
    test::Rng rng(seed);
    std::vector<uint8_t> data = rng.Bytes(blockSize);
    for (size_t i = 0; i < 300; i++) data.push_back(data[i]);
    std::vector<uint8_t> tail = rng.Bytes(100);
    data.insert(data.end(), tail.begin(), tail.end());
    for (size_t i = 0; i < 258; i++) data.push_back(data[blockSize - 258 + i]);
    return data;
}

std::vector<uint8_t> Mutate(const std::vector<uint8_t>& original, test::Rng& rng) {
    std::vector<uint8_t> data = original;
    int kind = (int)rng.Below(7);
    if (data.empty()) kind = 3;
    switch (kind) {
        case 0: {
            int flips = 1 + (int)rng.Below(3);
            for (int i = 0; i < flips; i++) data[rng.Below((uint32_t)data.size())] ^= (uint8_t)(1u << rng.Below(8));
            break;
        }
        case 1:
            data[rng.Below((uint32_t)data.size())] = (uint8_t)rng.Next();
            break;
        case 2:
            data.erase(data.begin() + (std::ptrdiff_t)rng.Below((uint32_t)data.size()));
            break;
        case 3:
            data.insert(data.begin() + (std::ptrdiff_t)rng.Below((uint32_t)data.size() + 1), (uint8_t)rng.Next());
            break;
        case 4:
            data.resize(rng.Below((uint32_t)data.size() + 1));
            break;
        case 5: {
            size_t from = rng.Below((uint32_t)data.size());
            size_t length = 1 + rng.Below(16);
            if (from + length > data.size()) length = data.size() - from;
            std::vector<uint8_t> chunk(data.begin() + (std::ptrdiff_t)from, data.begin() + (std::ptrdiff_t)(from + length));
            data.insert(data.begin() + (std::ptrdiff_t)from, chunk.begin(), chunk.end());
            break;
        }
        default: {
            size_t a = rng.Below((uint32_t)data.size());
            size_t b = rng.Below((uint32_t)data.size());
            std::swap(data[a], data[b]);
            break;
        }
    }
    return data;
}

void CheckDecompressInvariants(const std::vector<uint8_t>& input, uint64_t rawSize) {
    std::vector<uint8_t> out;
    bool ok = PackageCompression::Decompress(kDeflate, input, rawSize, out);
    if (ok) {
        CHECK_MSG(out.size() == rawSize, "success must produce exactly rawSize bytes");
    } else {
        CHECK_MSG(out.empty(), "failure must leave no output");
    }
}

class XorTestCompressor : public IPackCompressor {
public:
    bool Compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) const override {
        output = input;
        for (uint8_t& b : output) b ^= 0x55;
        return true;
    }
    bool Decompress(const std::vector<uint8_t>& input, uint64_t rawSize, std::vector<uint8_t>& output) const override {
        if (input.size() != rawSize) return false;
        output = input;
        for (uint8_t& b : output) b ^= 0x55;
        return true;
    }
};

}

TEST(inflate_decodes_zlib_streams) {
    for (const vectors::InflateVector& v : vectors::kInflate) {
        std::vector<uint8_t> input = VectorBytes(v);
        std::vector<uint8_t> out;
        bool ok = PackageCompression::Decompress(kDeflate, input, v.rawSize, out);
        CHECK_MSG(ok, std::string("vector ") + v.name);
        if (!ok) continue;
        CHECK_MSG(out.size() == v.rawSize, std::string("size of vector ") + v.name);
        CHECK_MSG(DigestHex(out) == v.rawSha256, std::string("content of vector ") + v.name);
    }
}

TEST(inflate_rejects_wrong_raw_size) {
    for (const vectors::InflateVector& v : vectors::kInflate) {
        std::vector<uint8_t> input = VectorBytes(v);
        std::vector<uint8_t> out;
        CHECK_MSG(!PackageCompression::Decompress(kDeflate, input, v.rawSize + 1, out), std::string("too large: ") + v.name);
        CHECK(out.empty());
        if (v.rawSize > 1) {
            CHECK_MSG(!PackageCompression::Decompress(kDeflate, input, v.rawSize - 1, out), std::string("too small: ") + v.name);
            CHECK(out.empty());
        }
        CHECK_MSG(!PackageCompression::Decompress(kDeflate, input, 0, out), std::string("zero: ") + v.name);
        CHECK(out.empty());
    }
}

TEST(inflate_rejects_every_truncation) {
    for (const vectors::InflateVector& v : vectors::kInflate) {
        std::vector<uint8_t> full = VectorBytes(v);
        size_t step = full.size() > 600 ? 1 + full.size() / 400 : 1;
        for (size_t length = 0; length < full.size(); length += (length < 200 ? 1 : step)) {
            std::vector<uint8_t> prefix(full.begin(), full.begin() + (std::ptrdiff_t)length);
            std::vector<uint8_t> out;
            bool ok = PackageCompression::Decompress(kDeflate, prefix, v.rawSize, out);
            CHECK_MSG(!ok, std::string("truncated to ") + std::to_string(length) + " of " + std::to_string(full.size()) + ": " + v.name);
            CHECK(out.empty());
        }
        std::vector<uint8_t> lastByteMissing(full.begin(), full.end() - 1);
        std::vector<uint8_t> out;
        CHECK_MSG(!PackageCompression::Decompress(kDeflate, lastByteMissing, v.rawSize, out), std::string("last byte missing: ") + v.name);
    }
}

TEST(inflate_survives_mutated_streams) {
    test::Rng rng(20240611);
    for (const vectors::InflateVector& v : vectors::kInflate) {
        std::vector<uint8_t> original = VectorBytes(v);
        int iterations = v.rawSize <= 20000 ? 1500 : 250;
        for (int i = 0; i < iterations; i++) {
            CheckDecompressInvariants(Mutate(original, rng), v.rawSize);
        }
    }
}

TEST(inflate_survives_mutated_streams_with_wrong_sizes) {
    test::Rng rng(777);
    for (const vectors::InflateVector& v : vectors::kInflate) {
        std::vector<uint8_t> original = VectorBytes(v);
        int iterations = v.rawSize <= 20000 ? 400 : 80;
        for (int i = 0; i < iterations; i++) {
            std::vector<uint8_t> input = Mutate(original, rng);
            uint64_t size = 1 + rng.Below((uint32_t)(v.rawSize + 64));
            CheckDecompressInvariants(input, size);
        }
    }
}

TEST(inflate_survives_random_garbage) {
    test::Rng rng(31337);
    for (int i = 0; i < 6000; i++) {
        std::vector<uint8_t> input = rng.Bytes(1 + rng.Below(96));
        uint64_t limit = (uint64_t)input.size() * 1032;
        uint64_t size = 1 + (rng.Next() % limit);
        CheckDecompressInvariants(input, size);
    }
}

TEST(inflate_survives_structured_garbage) {
    test::Rng rng(4242);
    for (int i = 0; i < 3000; i++) {
        std::vector<uint8_t> input = rng.Bytes(1 + rng.Below(48));
        input[0] = (uint8_t)((input[0] & 0xF8) | (rng.Below(4) << 1) | rng.Below(2));
        uint64_t size = 1 + rng.Below(4096);
        CheckDecompressInvariants(input, size);
    }
}

TEST(inflate_rejects_absurd_sizes_without_allocating) {
    std::vector<uint8_t> tiny = { 0x03, 0x00 };
    std::vector<uint8_t> out;
    CHECK(!PackageCompression::Decompress(kDeflate, tiny, 1ull << 40, out));
    CHECK(!PackageCompression::Decompress(kDeflate, tiny, UINT64_MAX, out));
    CHECK(!PackageCompression::Decompress(kDeflate, tiny, 1033ull * 2 + 1, out));
    CHECK(out.empty());

    std::vector<uint8_t> big(2200000, 0);
    CHECK(!PackageCompression::Decompress(kDeflate, big, 0x80000000ull, out));
    CHECK(!PackageCompression::Decompress(kDeflate, big, UINT64_MAX, out));
    CHECK(out.empty());

    std::vector<uint8_t> empty;
    CHECK(!PackageCompression::Decompress(kDeflate, empty, 10, out));
}

TEST(inflate_handcrafted_valid_streams) {
    std::vector<uint8_t> out;

    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 257);
        FixedDistance(bits, 0);
        FixedSymbol(bits, 256);
        CHECK(InflateOk(bits.Finish(), 4, out));
        CHECK_BYTES(out, std::vector<uint8_t>({ 'a', 'a', 'a', 'a' }));
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 'b');
        FixedSymbol(bits, 258);
        FixedDistance(bits, 1);
        FixedSymbol(bits, 256);
        CHECK(InflateOk(bits.Finish(), 6, out));
        CHECK_BYTES(out, std::vector<uint8_t>({ 'a', 'b', 'a', 'b', 'a', 'b' }));
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        for (char c : std::string("abcde")) FixedSymbol(bits, (uint32_t)c);
        FixedSymbol(bits, 257);
        FixedDistance(bits, 4);
        bits.Lsb(0, 1);
        FixedSymbol(bits, 256);
        CHECK(InflateOk(bits.Finish(), 8, out));
        CHECK_BYTES(out, std::vector<uint8_t>({ 'a', 'b', 'c', 'd', 'e', 'a', 'b', 'c' }));
    }
    {
        CHECK(InflateOk(StoredBlock({ 1, 2, 3 }, true), 3, out));
        CHECK_BYTES(out, std::vector<uint8_t>({ 1, 2, 3 }));
    }
    {
        std::vector<uint8_t> first = StoredBlock({ 1, 2 }, false);
        std::vector<uint8_t> second = StoredBlock({ 3 }, true);
        first.insert(first.end(), second.begin(), second.end());
        CHECK(InflateOk(first, 3, out));
        CHECK_BYTES(out, std::vector<uint8_t>({ 1, 2, 3 }));
    }
}

TEST(inflate_rejects_handcrafted_invalid_streams) {
    std::vector<uint8_t> out;

    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 257);
        FixedDistance(bits, 4);
        bits.Lsb(0, 1);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 4, out), "distance reaches before the start of the output");
        CHECK(out.empty());
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 'b');
        FixedSymbol(bits, 257);
        FixedDistance(bits, 2);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 5, out), "distance one past the start of the output");
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 257);
        FixedDistance(bits, 0);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 3, out), "match as the very first symbol");
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 257);
        FixedDistance(bits, 0);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 3, out), "match longer than the declared output");
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 'b');
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 1, out), "literals beyond the declared output");
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 286);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 10, out), "invalid length symbol 286");
    }
    {
        DeflateBits bits;
        FixedHeader(bits, true);
        FixedSymbol(bits, 'a');
        FixedSymbol(bits, 257);
        FixedDistance(bits, 30);
        FixedSymbol(bits, 256);
        CHECK_MSG(!InflateOk(bits.Finish(), 4, out), "invalid distance symbol 30");
    }
    {
        DeflateBits bits;
        bits.Lsb(1, 1);
        bits.Lsb(3, 2);
        bits.Lsb(0, 16);
        CHECK_MSG(!InflateOk(bits.Finish(), 1, out), "reserved block type");
    }
    CHECK_MSG(!InflateOk(StoredBlock({ 1, 2, 3 }, true, 0, false, true), 3, out), "stored block with a wrong complement");
    CHECK_MSG(!InflateOk(StoredBlock({ 1, 2, 3 }, true), 2, out), "stored block longer than the declared output");
    CHECK_MSG(!InflateOk(StoredBlock({ 1, 2, 3 }, true, 5, true), 5, out), "stored block longer than its data");
    CHECK_MSG(!InflateOk(StoredBlock({}, true), 1, out), "empty stream for a non-empty output");
    {
        std::vector<uint8_t> notFinal = StoredBlock({ 1, 2, 3 }, false);
        CHECK_MSG(!InflateOk(notFinal, 3, out), "stream that never reaches a final block");
    }
    {
        DeflateBits bits;
        bits.Lsb(1, 1);
        bits.Lsb(2, 2);
        bits.Lsb(31, 5);
        bits.Lsb(0, 5);
        bits.Lsb(0, 4);
        for (int i = 0; i < 4; i++) bits.Lsb(1, 3);
        CHECK_MSG(!InflateOk(bits.Finish(), 1, out), "too many literal/length codes");
    }
    {
        DeflateBits bits;
        bits.Lsb(1, 1);
        bits.Lsb(2, 2);
        bits.Lsb(0, 5);
        bits.Lsb(30, 5);
        bits.Lsb(0, 4);
        for (int i = 0; i < 4; i++) bits.Lsb(1, 3);
        CHECK_MSG(!InflateOk(bits.Finish(), 1, out), "too many distance codes");
    }
    {
        DeflateBits bits;
        bits.Lsb(1, 1);
        bits.Lsb(2, 2);
        bits.Lsb(0, 5);
        bits.Lsb(0, 5);
        bits.Lsb(0, 4);
        for (int i = 0; i < 4; i++) bits.Lsb(1, 3);
        bits.Lsb(0, 8);
        CHECK_MSG(!InflateOk(bits.Finish(), 1, out), "over-subscribed code length code");
    }
}

TEST(inflate_never_requests_memory_beyond_the_expansion_limit) {
    std::vector<uint8_t> tiny = { 0x03, 0x00 };
    std::vector<uint8_t> out;

    g_largestRequest = 0;
    g_trackRequests = true;
    bool ok = PackageCompression::Decompress(kDeflate, tiny, 0x7FFFFFFFull, out);
    ok = ok || PackageCompression::Decompress(kDeflate, tiny, 100000000ull, out);
    ok = ok || PackageCompression::Decompress(kDeflate, tiny, 5000000ull, out);
    g_trackRequests = false;

    CHECK(!ok);
    CHECK_MSG(g_largestRequest.load() < 1024 * 1024, "largest allocation request was " + std::to_string(g_largestRequest.load()) + " bytes");

    std::vector<uint8_t> input(1000, 0x55);
    g_largestRequest = 0;
    g_trackRequests = true;
    PackageCompression::Decompress(kDeflate, input, 1000ull * 1032, out);
    PackageCompression::Decompress(kDeflate, input, 1000ull * 1032 + 1, out);
    g_trackRequests = false;
    CHECK_MSG(g_largestRequest.load() <= 1000ull * 1032 + 4096, "allocation stays within input size times the deflate limit");
}

TEST(inflate_expansion_stays_within_deflate_limit) {
    const vectors::InflateVector* run = FindVector("single_byte_run");
    REQUIRE(run != nullptr);
    CHECK((uint64_t)run->size * 1032 >= run->rawSize);
    std::vector<uint8_t> input = VectorBytes(*run);
    std::vector<uint8_t> out;
    CHECK(PackageCompression::Decompress(kDeflate, input, run->rawSize, out));
    CHECK(out.size() == run->rawSize);
}

TEST(compression_none_method_rules) {
    std::vector<uint8_t> data = { 1, 2, 3, 4, 5 };
    std::vector<uint8_t> out;
    CHECK(PackageCompression::Decompress(kNone, data, 5, out));
    CHECK_BYTES(out, data);
    CHECK(!PackageCompression::Decompress(kNone, data, 4, out));
    CHECK(out.empty());
    CHECK(!PackageCompression::Decompress(kNone, data, 6, out));
    CHECK(PackageCompression::Decompress("", data, 5, out));
    CHECK_BYTES(out, data);

    std::vector<uint8_t> empty;
    CHECK(PackageCompression::Decompress(kNone, empty, 0, out));
    CHECK(out.empty());
}

TEST(compression_unknown_method_fails_cleanly) {
    std::vector<uint8_t> data = { 1, 2, 3 };
    std::vector<uint8_t> out = { 9, 9 };
    CHECK(!PackageCompression::Decompress("NoSuchMethod", data, 3, out));
    CHECK(out.empty());
    out = { 9, 9 };
    CHECK(!PackageCompression::Compress("NoSuchMethod", data, out));
    CHECK(out.empty());
}

TEST(compression_deflate_round_trips_varied_data) {
    struct Sample { std::string name; std::vector<uint8_t> data; };
    std::vector<Sample> samples;
    test::Rng rng(99);

    for (size_t size : { (size_t)1, (size_t)2, (size_t)3, (size_t)255, (size_t)256, (size_t)257, (size_t)4096, (size_t)65535, (size_t)65536, (size_t)65537, (size_t)200000 }) {
        samples.push_back({ "zeros " + std::to_string(size), std::vector<uint8_t>(size, 0) });
        samples.push_back({ "random " + std::to_string(size), rng.Bytes(size) });
        samples.push_back({ "text " + std::to_string(size), TextLike(size, size + 5) });
        std::vector<uint8_t> ramp(size);
        for (size_t i = 0; i < size; i++) ramp[i] = (uint8_t)(i * 3);
        samples.push_back({ "ramp " + std::to_string(size), ramp });
    }
    samples.push_back({ "far copy 32000", FarCopy(32000, 7) });
    samples.push_back({ "far copy 20000", FarCopy(20000, 8) });
    samples.push_back({ "zeros 3000000", std::vector<uint8_t>(3000000, 0) });

    for (const Sample& sample : samples) {
        std::vector<uint8_t> packed;
        bool compressed = PackageCompression::Compress(kDeflate, sample.data, packed);
        CHECK_MSG(compressed, "compress " + sample.name);
        if (!compressed) continue;
        std::vector<uint8_t> restored;
        bool ok = PackageCompression::Decompress(kDeflate, packed, sample.data.size(), restored);
        CHECK_MSG(ok, "decompress " + sample.name);
        if (!ok) continue;
        CHECK_MSG(test::BytesEqual(restored, sample.data), "content " + sample.name);
    }
}

TEST(compression_deflate_shrinks_repetitive_data) {
    std::vector<uint8_t> data(100000, 'x');
    std::vector<uint8_t> packed;
    REQUIRE(PackageCompression::Compress(kDeflate, data, packed));
    CHECK(packed.size() < data.size() / 50);

    std::vector<uint8_t> text = TextLike(100000, 3);
    REQUIRE(PackageCompression::Compress(kDeflate, text, packed));
    CHECK(packed.size() < text.size() / 2);
}

TEST(compression_deflate_rejects_empty_input) {
    std::vector<uint8_t> empty;
    std::vector<uint8_t> out = { 1 };
    CHECK(!PackageCompression::Compress(kDeflate, empty, out));
    CHECK(out.empty());
}

TEST(compression_none_has_no_compressor) {
    std::vector<uint8_t> data = { 1, 2, 3 };
    std::vector<uint8_t> out;
    CHECK(!PackageCompression::Compress(kNone, data, out));
}

TEST(compression_registry_rules) {
    PackCompressionInfo info;
    CHECK(PackCompressionRegistry::Get().Find(kNone, info));
    CHECK(!info.compressor);
    CHECK(PackCompressionRegistry::Get().Find(kDeflate, info));
    CHECK(info.compressor != nullptr);
    CHECK(PackCompressionRegistry::Get().IsAvailable(info));
    CHECK(!PackCompressionRegistry::Get().Find("NoSuchMethod", info));

    size_t before = PackCompressionRegistry::Get().GetAll().size();

    PackCompressionInfo noId;
    noId.compressor = std::make_shared<XorTestCompressor>();
    PackCompressionRegistry::Get().Register(noId);
    PackCompressionInfo noCompressor;
    noCompressor.id = "TestNoCompressor";
    PackCompressionRegistry::Get().Register(noCompressor);
    CHECK(PackCompressionRegistry::Get().GetAll().size() == before);

    PackCompressionInfo custom;
    custom.id = "TestXorCompression";
    custom.displayName = "first";
    custom.compressor = std::make_shared<XorTestCompressor>();
    PackCompressionRegistry::Get().Register(custom);
    CHECK(PackCompressionRegistry::Get().GetAll().size() == before + 1);
    custom.displayName = "second";
    PackCompressionRegistry::Get().Register(custom);
    CHECK(PackCompressionRegistry::Get().GetAll().size() == before + 1);

    std::vector<uint8_t> data = { 10, 20, 30, 40 };
    std::vector<uint8_t> packed, restored;
    CHECK(PackageCompression::Compress("TestXorCompression", data, packed));
    CHECK(!test::BytesEqual(packed, data));
    CHECK(PackageCompression::Decompress("TestXorCompression", packed, data.size(), restored));
    CHECK_BYTES(restored, data);
}

TEST_MAIN()
