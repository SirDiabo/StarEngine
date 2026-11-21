#include "TestHelper.h"
#include "Core/BakedKey.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> Entropy(unsigned seed) {
    test::Rng rng(seed + 1);
    return rng.Bytes(32);
}

bool Contains(const std::vector<uint8_t>& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

std::string LongKey(size_t length, unsigned seed) {
    test::Rng rng(seed + 77);
    std::string key;
    for (size_t i = 0; i < length; i++) key.push_back("0123456789abcdef"[rng.Below(16)]);
    return key;
}

}

TEST(baked_key_round_trips_for_many_layouts) {
    for (unsigned seed = 0; seed < 300; seed++) {
        BakedKey::Material material;
        material.cipher = seed % 3 == 0 ? "ChaCha12" : (seed % 3 == 1 ? "X" : "A-Module-Defined-Method");
        material.key = LongKey(1 + (seed * 7) % 200, seed);

        std::vector<uint8_t> blob;
        REQUIRE(BakedKey::Encode(material, Entropy(seed), blob));
        CHECK(blob.size() > material.key.size());

        BakedKey::Material decoded;
        CHECK_MSG(BakedKey::Decode(blob.data(), blob.size(), decoded), "seed " + std::to_string(seed));
        CHECK_EQ_STR(decoded.cipher, material.cipher);
        CHECK_EQ_STR(decoded.key, material.key);
    }
}

TEST(baked_key_supports_the_size_limits) {
    BakedKey::Material material;
    material.cipher = std::string(BakedKey::kMaxCipherName, 'c');
    material.key = std::string(BakedKey::kMaxKeyBytes, 'k');
    std::vector<uint8_t> blob;
    REQUIRE(BakedKey::Encode(material, Entropy(1), blob));
    BakedKey::Material decoded;
    REQUIRE(BakedKey::Decode(blob.data(), blob.size(), decoded));
    CHECK_EQ_STR(decoded.cipher, material.cipher);
    CHECK_EQ_STR(decoded.key, material.key);

    material.key = "k";
    material.cipher = "c";
    REQUIRE(BakedKey::Encode(material, Entropy(2), blob));
    REQUIRE(BakedKey::Decode(blob.data(), blob.size(), decoded));
    CHECK_EQ_STR(decoded.key, "k");
}

TEST(baked_key_layout_is_deterministic_per_entropy_and_differs_between_builds) {
    BakedKey::Material material{ "ChaCha12", LongKey(64, 5) };
    std::vector<uint8_t> a, b, c;
    REQUIRE(BakedKey::Encode(material, Entropy(10), a));
    REQUIRE(BakedKey::Encode(material, Entropy(10), b));
    REQUIRE(BakedKey::Encode(material, Entropy(11), c));
    CHECK_BYTES(a, b);
    CHECK(!test::BytesEqual(a, c));

    int differentSizes = 0;
    std::vector<uint8_t> first;
    for (unsigned seed = 0; seed < 40; seed++) {
        std::vector<uint8_t> blob;
        REQUIRE(BakedKey::Encode(material, Entropy(100 + seed), blob));
        if (seed == 0) first = blob;
        else if (blob.size() != first.size()) differentSizes++;
    }
    CHECK(differentSizes > 20);
}

TEST(baked_key_never_stores_the_key_or_cipher_in_one_piece) {
    for (unsigned seed = 0; seed < 200; seed++) {
        BakedKey::Material material{ "ChaCha12", LongKey(64, seed) };
        std::vector<uint8_t> blob;
        REQUIRE(BakedKey::Encode(material, Entropy(seed), blob));
        CHECK_MSG(!Contains(blob, material.key), "key visible, seed " + std::to_string(seed));
        CHECK_MSG(!Contains(blob, material.key.substr(0, 12)), "key prefix visible, seed " + std::to_string(seed));
        CHECK_MSG(!Contains(blob, material.cipher), "cipher visible, seed " + std::to_string(seed));
    }
}

TEST(baked_key_encode_rejects_bad_input) {
    std::vector<uint8_t> blob = { 1, 2, 3 };
    BakedKey::Material good{ "ChaCha12", "abcdef" };

    CHECK(!BakedKey::Encode(good, std::vector<uint8_t>(BakedKey::kMinEntropyBytes - 1, 7), blob));
    CHECK(blob.empty());
    CHECK(!BakedKey::Encode(good, std::vector<uint8_t>(), blob));
    CHECK(!BakedKey::Encode({ "", "abcdef" }, Entropy(1), blob));
    CHECK(!BakedKey::Encode({ "ChaCha12", "" }, Entropy(1), blob));
    CHECK(!BakedKey::Encode({ std::string(BakedKey::kMaxCipherName + 1, 'c'), "k" }, Entropy(1), blob));
    CHECK(!BakedKey::Encode({ "c", std::string(BakedKey::kMaxKeyBytes + 1, 'k') }, Entropy(1), blob));
    CHECK(blob.empty());
    CHECK(BakedKey::Encode(good, std::vector<uint8_t>(BakedKey::kMinEntropyBytes, 7), blob));
}

TEST(baked_key_decode_rejects_every_truncation) {
    BakedKey::Material material{ "ChaCha12", LongKey(64, 9) };
    std::vector<uint8_t> blob;
    REQUIRE(BakedKey::Encode(material, Entropy(9), blob));

    BakedKey::Material decoded;
    for (size_t length = 0; length < blob.size(); length++) {
        decoded = BakedKey::Material();
        std::vector<uint8_t> cut(blob.begin(), blob.begin() + (std::ptrdiff_t)length);
        const bool ok = BakedKey::Decode(cut.data(), cut.size(), decoded);
        if (ok) CHECK_MSG(decoded.key == material.key && decoded.cipher == material.cipher, "length " + std::to_string(length));
    }
    CHECK(!BakedKey::Decode(nullptr, 10, decoded));
    CHECK(!BakedKey::Decode(blob.data(), 0, decoded));
    CHECK(!BakedKey::Decode(blob.data(), 3, decoded));
}

TEST(baked_key_decode_succeeds_only_with_the_original_material) {
    BakedKey::Material material{ "ChaCha12", LongKey(48, 3) };
    std::vector<uint8_t> blob;
    REQUIRE(BakedKey::Encode(material, Entropy(3), blob));

    int rejected = 0;
    for (size_t i = 0; i < blob.size(); i++) {
        for (int bit = 0; bit < 8; bit += 2) {
            std::vector<uint8_t> damaged = blob;
            damaged[i] ^= (uint8_t)(1 << bit);
            BakedKey::Material decoded;
            if (BakedKey::Decode(damaged.data(), damaged.size(), decoded)) {
                CHECK_MSG(decoded.key == material.key && decoded.cipher == material.cipher, "byte " + std::to_string(i));
            } else {
                rejected++;
            }
        }
    }
    CHECK(rejected > 100);
}

TEST(baked_key_decode_survives_random_blobs) {
    test::Rng rng(2024);
    for (int round = 0; round < 3000; round++) {
        std::vector<uint8_t> blob = rng.Bytes(rng.Below(300));
        BakedKey::Material decoded;
        BakedKey::Decode(blob.data(), blob.size(), decoded);
    }

    BakedKey::Material material{ "ChaCha12", LongKey(40, 4) };
    std::vector<uint8_t> good;
    REQUIRE(BakedKey::Encode(material, Entropy(4), good));
    for (int round = 0; round < 3000; round++) {
        std::vector<uint8_t> blob = good;
        const int edits = 1 + (int)rng.Below(5);
        for (int e = 0; e < edits; e++) blob[rng.Below((uint32_t)blob.size())] = (uint8_t)rng.Next();
        BakedKey::Material decoded;
        if (BakedKey::Decode(blob.data(), blob.size(), decoded)) {
            CHECK(!decoded.cipher.empty());
            CHECK(!decoded.key.empty());
        }
    }
}

TEST(baked_key_header_text_lists_the_blob_bytes) {
    BakedKey::Material material{ "ChaCha12", LongKey(33, 6) };
    std::vector<uint8_t> blob;
    REQUIRE(BakedKey::Encode(material, Entropy(6), blob));

    const std::string text = BakedKey::BuildHeaderText(blob);
    CHECK(text.find("#pragma once") == 0);
    CHECK(text.find("kBlob[]") != std::string::npos);
    CHECK(text.find("kBlobSize") != std::string::npos);

    std::vector<uint8_t> parsed;
    size_t pos = 0;
    while ((pos = text.find("0x", pos)) != std::string::npos) {
        parsed.push_back((uint8_t)std::stoul(text.substr(pos + 2, 2), nullptr, 16));
        pos += 4;
    }
    CHECK_BYTES(parsed, blob);

    const std::string empty = BakedKey::BuildEmptyHeaderText();
    CHECK(empty.find("kBlobSize = 0") != std::string::npos);
    CHECK(empty.find("#pragma once") == 0);
}

TEST(baked_key_get_reports_no_key_without_a_generated_header) {
    BakedKey::Material material;
    CHECK(!BakedKey::Get(material));
    CHECK(material.key.empty());
}

TEST_MAIN()
