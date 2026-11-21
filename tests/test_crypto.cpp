#include "TestHelper.h"
#include "Core/ChaCha12.h"
#include "Core/Crc32.h"
#include "Core/PackageCipher.h"
#include "Core/Sha256.h"
#include "data/CryptoVectors.h"
#include <cctype>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> Pattern(size_t count, unsigned seed) {
    std::vector<uint8_t> out(count);
    for (size_t i = 0; i < count; i++) out[i] = (uint8_t)((i * 7 + seed) & 0xFF);
    return out;
}

std::string DigestHex(const std::vector<uint8_t>& data) {
    return Sha256::ToHex(Sha256::Compute(data));
}

std::vector<uint8_t> ShaMessage(const vectors::ShaVector& v) {
    std::string name = v.name;
    if (name == "abc") return test::FromHex("616263");
    if (name == "two block") {
        std::string text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        return std::vector<uint8_t>(text.begin(), text.end());
    }
    if (v.fill == 1) return std::vector<uint8_t>((size_t)v.length, (uint8_t)'a');
    return Pattern((size_t)v.length, v.seed);
}

std::vector<uint8_t> Bytes(const char* hex) {
    return test::FromHex(hex);
}

}

TEST(sha256_known_answers) {
    for (const vectors::ShaVector& v : vectors::kSha) {
        std::vector<uint8_t> message = ShaMessage(v);
        REQUIRE(message.size() == v.length);
        CHECK_EQ_STR(DigestHex(message), v.digest);
    }
}

TEST(sha256_incremental_matches_one_shot) {
    test::Rng rng(12345);
    for (size_t length = 0; length <= 300; length++) {
        std::vector<uint8_t> message = Pattern(length, 11);
        Sha256::Digest expected = Sha256::Compute(message);

        Sha256::Hasher byteWise;
        for (uint8_t b : message) byteWise.Update(&b, 1);
        CHECK(Sha256::Equal(byteWise.Final(), expected));

        for (int round = 0; round < 4; round++) {
            Sha256::Hasher chunked;
            size_t pos = 0;
            while (pos < length) {
                size_t take = 1 + rng.Below(97);
                if (take > length - pos) take = length - pos;
                chunked.Update(message.data() + pos, take);
                pos += take;
            }
            CHECK_MSG(Sha256::Equal(chunked.Final(), expected), "length " + std::to_string(length));
        }
    }
}

TEST(sha256_reset_reuses_hasher) {
    std::vector<uint8_t> first = Pattern(130, 1);
    std::vector<uint8_t> second = Pattern(77, 2);
    Sha256::Hasher hasher;
    hasher.Update(first.data(), first.size());
    Sha256::Digest a = hasher.Final();
    hasher.Reset();
    hasher.Update(second.data(), second.size());
    Sha256::Digest b = hasher.Final();
    CHECK(Sha256::Equal(a, Sha256::Compute(first)));
    CHECK(Sha256::Equal(b, Sha256::Compute(second)));
}

TEST(sha256_overloads_agree) {
    std::string text = "overload check";
    std::vector<uint8_t> bytes(text.begin(), text.end());
    Sha256::Digest a = Sha256::Compute(text);
    Sha256::Digest b = Sha256::Compute(bytes);
    Sha256::Digest c = Sha256::Compute(text.data(), text.size());
    CHECK(Sha256::Equal(a, b));
    CHECK(Sha256::Equal(a, c));
}

TEST(sha256_hex_helpers) {
    Sha256::Digest digest = Sha256::Compute(std::string("abc"));
    std::string hex = Sha256::ToHex(digest);
    CHECK_EQ_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    Sha256::Digest parsed;
    CHECK(Sha256::FromHex(hex, parsed));
    CHECK(Sha256::Equal(parsed, digest));

    std::string upper = hex;
    for (char& c : upper) c = (char)std::toupper((unsigned char)c);
    Sha256::Digest parsedUpper;
    CHECK(Sha256::FromHex(upper, parsedUpper));
    CHECK(Sha256::Equal(parsedUpper, digest));

    Sha256::Digest scratch;
    CHECK(!Sha256::FromHex("", scratch));
    CHECK(!Sha256::FromHex(hex.substr(1), scratch));
    CHECK(!Sha256::FromHex(hex + "0", scratch));
    std::string badChar = hex;
    badChar[10] = 'g';
    CHECK(!Sha256::FromHex(badChar, scratch));
    std::string spaced = hex;
    spaced[63] = ' ';
    CHECK(!Sha256::FromHex(spaced, scratch));
}

TEST(sha256_equal_detects_single_bit_differences) {
    Sha256::Digest base = Sha256::Compute(std::string("base"));
    CHECK(Sha256::Equal(base, base));
    for (int byte = 0; byte < 32; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            Sha256::Digest other = base;
            other.bytes[byte] = (uint8_t)(other.bytes[byte] ^ (1 << bit));
            CHECK(!Sha256::Equal(base, other));
        }
    }
}

TEST(hmac_sha256_known_answers) {
    for (const vectors::HmacVector& v : vectors::kHmac) {
        std::vector<uint8_t> key = Bytes(v.key);
        std::vector<uint8_t> data = Bytes(v.data);
        Sha256::Digest mac = Sha256::Hmac(key.data(), key.size(), data.data(), data.size());
        CHECK_EQ_STR(Sha256::ToHex(mac), test::Hex(Bytes(v.mac).data(), 32));
    }
}

TEST(hmac_sha256_depends_on_key_and_data) {
    std::vector<uint8_t> data = Pattern(50, 1);
    std::vector<uint8_t> keyA = Pattern(20, 2);
    std::vector<uint8_t> keyB = keyA;
    keyB[5] ^= 1;
    std::vector<uint8_t> dataB = data;
    dataB[49] ^= 0x80;
    Sha256::Digest base = Sha256::Hmac(keyA.data(), keyA.size(), data.data(), data.size());
    CHECK(!Sha256::Equal(base, Sha256::Hmac(keyB.data(), keyB.size(), data.data(), data.size())));
    CHECK(!Sha256::Equal(base, Sha256::Hmac(keyA.data(), keyA.size(), dataB.data(), dataB.size())));
    CHECK(Sha256::Equal(base, Sha256::Hmac(keyA.data(), keyA.size(), data.data(), data.size())));
}

TEST(chacha12_block_known_answers) {
    for (const vectors::ChaChaBlockVector& v : vectors::kChaChaBlocks) {
        std::vector<uint8_t> key = Bytes(v.key);
        std::vector<uint8_t> nonce = Bytes(v.nonce);
        std::vector<uint8_t> expected = Bytes(v.block);
        REQUIRE(key.size() == 32 && nonce.size() == 8 && expected.size() == 64);

        std::vector<uint8_t> out(64);
        REQUIRE(ChaCha12::BlockWithRounds(key.data(), nonce.data(), v.counter, v.rounds, out.data()));
        CHECK_MSG(test::BytesEqual(out, expected), "rounds " + std::to_string(v.rounds) + " counter " + std::to_string(v.counter));

        if (v.rounds == ChaCha12::kRounds) {
            std::vector<uint8_t> viaBlock(64);
            ChaCha12::Block(key.data(), nonce.data(), v.counter, viaBlock.data());
            CHECK_BYTES(viaBlock, expected);
        }
    }
}

TEST(chacha_published_test_vectors_for_8_12_and_20_rounds) {
    struct Published { int rounds; const char* block; };
    const Published published[] = {
        { 8, "3e00ef2f895f40d67f5bb8e81f09a5a12c840ec3ce9a7f3b181be188ef711a1e984ce172b9216f419f445367456d5619314a42a3da86b001387bfdb80e0cfe42" },
        { 12, "9bf49a6a0755f953811fce125f2683d50429c3bb49e074147e0089a52eae155f0564f879d27ae3c02ce82834acfa8c793a629f2ca0de6919610be82f411326be" },
        { 20, "76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586" },
    };
    std::vector<uint8_t> key(32, 0);
    std::vector<uint8_t> nonce(8, 0);
    for (const Published& p : published) {
        std::vector<uint8_t> out(64);
        REQUIRE(ChaCha12::BlockWithRounds(key.data(), nonce.data(), 0, p.rounds, out.data()));
        CHECK_MSG(test::BytesEqual(out, Bytes(p.block)), "rounds " + std::to_string(p.rounds));
    }

    std::vector<uint8_t> rfcKey(32);
    for (size_t i = 0; i < rfcKey.size(); i++) rfcKey[i] = (uint8_t)i;
    std::vector<uint8_t> rfcNonce = Bytes("0000004a00000000");
    std::vector<uint8_t> rfcBlock(64);
    REQUIRE(ChaCha12::BlockWithRounds(rfcKey.data(), rfcNonce.data(), 1ull | (0x09000000ull << 32), 20, rfcBlock.data()));
    CHECK_BYTES(std::vector<uint8_t>(rfcBlock.begin(), rfcBlock.begin() + 32),
                Bytes("10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"));
}

TEST(chacha_rejects_unsupported_round_counts) {
    std::vector<uint8_t> key(32, 1);
    std::vector<uint8_t> nonce(8, 2);
    std::vector<uint8_t> out(64, 0xAA);
    for (int rounds : { -2, 0, 1, 3, 13, 22, 100 }) {
        CHECK_MSG(!ChaCha12::BlockWithRounds(key.data(), nonce.data(), 0, rounds, out.data()), std::to_string(rounds));
    }
    CHECK_BYTES(out, std::vector<uint8_t>(64, 0xAA));
}

TEST(chacha12_xor_known_answers_and_symmetry) {
    for (const vectors::ChaChaXorVector& v : vectors::kChaChaXor) {
        std::vector<uint8_t> key = Bytes(v.key);
        std::vector<uint8_t> nonce = Bytes(v.nonce);
        std::vector<uint8_t> plain = Bytes(v.plain);
        std::vector<uint8_t> expected = Bytes(v.cipher);
        REQUIRE(key.size() == 32 && nonce.size() == 8 && plain.size() == expected.size());

        std::vector<uint8_t> data = plain;
        ChaCha12::Xor(key.data(), nonce.data(), v.counter, data.data(), data.size());
        CHECK_MSG(test::BytesEqual(data, expected), "size " + std::to_string(plain.size()));

        ChaCha12::Xor(key.data(), nonce.data(), v.counter, data.data(), data.size());
        CHECK_BYTES(data, plain);
    }
}

TEST(chacha12_keystream_is_prefix_stable) {
    std::vector<uint8_t> key = Pattern(32, 8);
    std::vector<uint8_t> nonce = Pattern(8, 3);

    std::vector<uint8_t> full(300, 0);
    ChaCha12::Xor(key.data(), nonce.data(), 0, full.data(), full.size());

    for (size_t length = 0; length <= 200; length++) {
        std::vector<uint8_t> prefix(length, 0);
        ChaCha12::Xor(key.data(), nonce.data(), 0, prefix.data(), prefix.size());
        std::vector<uint8_t> expectedPrefix(full.begin(), full.begin() + (std::ptrdiff_t)length);
        CHECK_MSG(test::BytesEqual(prefix, expectedPrefix), "length " + std::to_string(length));
    }
}

TEST(chacha12_xor_matches_block_stream) {
    std::vector<uint8_t> key = Pattern(32, 14);
    std::vector<uint8_t> nonce = Pattern(8, 15);

    std::vector<uint8_t> stream;
    for (uint64_t counter = 0; counter < 4; counter++) {
        std::vector<uint8_t> block(64);
        ChaCha12::Block(key.data(), nonce.data(), counter, block.data());
        stream.insert(stream.end(), block.begin(), block.end());
    }
    std::vector<uint8_t> zeros(230, 0);
    ChaCha12::Xor(key.data(), nonce.data(), 0, zeros.data(), zeros.size());
    CHECK_BYTES(zeros, std::vector<uint8_t>(stream.begin(), stream.begin() + 230));
}

TEST(chacha12_counter_carries_into_the_high_word) {
    std::vector<uint8_t> key = Pattern(32, 16);
    std::vector<uint8_t> nonce = Pattern(8, 17);

    std::vector<uint8_t> twoBlocks(128, 0);
    ChaCha12::Xor(key.data(), nonce.data(), 0xFFFFFFFFull, twoBlocks.data(), twoBlocks.size());

    std::vector<uint8_t> first(64), second(64);
    ChaCha12::Block(key.data(), nonce.data(), 0xFFFFFFFFull, first.data());
    ChaCha12::Block(key.data(), nonce.data(), 0x100000000ull, second.data());
    CHECK_BYTES(std::vector<uint8_t>(twoBlocks.begin(), twoBlocks.begin() + 64), first);
    CHECK_BYTES(std::vector<uint8_t>(twoBlocks.begin() + 64, twoBlocks.end()), second);

    std::vector<uint8_t> wrapped(128, 0);
    ChaCha12::Xor(key.data(), nonce.data(), 0xFFFFFFFFFFFFFFFFull, wrapped.data(), wrapped.size());
    std::vector<uint8_t> last(64), zeroBlock(64);
    ChaCha12::Block(key.data(), nonce.data(), 0xFFFFFFFFFFFFFFFFull, last.data());
    ChaCha12::Block(key.data(), nonce.data(), 0, zeroBlock.data());
    CHECK_BYTES(std::vector<uint8_t>(wrapped.begin(), wrapped.begin() + 64), last);
    CHECK_BYTES(std::vector<uint8_t>(wrapped.begin() + 64, wrapped.end()), zeroBlock);
}

TEST(chacha12_zero_length_is_a_no_op) {
    std::vector<uint8_t> key = Pattern(32, 9);
    std::vector<uint8_t> nonce = Pattern(8, 9);
    ChaCha12::Xor(key.data(), nonce.data(), 0, nullptr, 0);
    std::vector<uint8_t> untouched = Pattern(5, 1);
    std::vector<uint8_t> copy = untouched;
    ChaCha12::Xor(key.data(), nonce.data(), 0, copy.data(), 0);
    CHECK_BYTES(copy, untouched);
}

TEST(chacha12_depends_on_every_key_and_nonce_byte) {
    std::vector<uint8_t> key = Pattern(32, 21);
    std::vector<uint8_t> nonce = Pattern(8, 22);
    std::vector<uint8_t> base(64);
    ChaCha12::Block(key.data(), nonce.data(), 0, base.data());

    for (size_t i = 0; i < key.size(); i++) {
        std::vector<uint8_t> other = key;
        other[i] ^= 1;
        std::vector<uint8_t> out(64);
        ChaCha12::Block(other.data(), nonce.data(), 0, out.data());
        CHECK_MSG(!test::BytesEqual(out, base), "key byte " + std::to_string(i));
    }
    for (size_t i = 0; i < nonce.size(); i++) {
        std::vector<uint8_t> other = nonce;
        other[i] ^= 1;
        std::vector<uint8_t> out(64);
        ChaCha12::Block(key.data(), other.data(), 0, out.data());
        CHECK_MSG(!test::BytesEqual(out, base), "nonce byte " + std::to_string(i));
    }
}

TEST(crc32_known_answers) {
    for (const vectors::CrcVector& v : vectors::kCrc) {
        std::vector<uint8_t> data = Bytes(v.data);
        CHECK_MSG(Crc32::Compute(data) == v.crc, "length " + std::to_string(data.size()));
    }
    CHECK(Crc32::Compute(std::string("123456789")) == 0xCBF43926u);
    CHECK(Crc32::Compute(std::string()) == 0u);
    CHECK(Crc32::Compute(nullptr, 0) == 0u);
}

TEST(crc32_incremental_matches_one_shot) {
    test::Rng rng(777);
    for (size_t length = 0; length <= 300; length++) {
        std::vector<uint8_t> data = Pattern(length, 5);
        uint32_t expected = Crc32::Compute(data);

        uint32_t byteWise = 0;
        for (uint8_t b : data) byteWise = Crc32::Update(byteWise, &b, 1);
        CHECK_MSG(byteWise == expected, "byte-wise length " + std::to_string(length));

        uint32_t chunked = 0;
        size_t pos = 0;
        while (pos < length) {
            size_t take = 1 + rng.Below(23);
            if (take > length - pos) take = length - pos;
            chunked = Crc32::Update(chunked, data.data() + pos, take);
            pos += take;
        }
        CHECK_MSG(chunked == expected, "chunked length " + std::to_string(length));
    }
}

TEST(crc32_detects_every_single_bit_flip) {
    std::vector<uint8_t> data = Pattern(97, 3);
    uint32_t base = Crc32::Compute(data);
    for (size_t byte = 0; byte < data.size(); byte++) {
        for (int bit = 0; bit < 8; bit++) {
            std::vector<uint8_t> other = data;
            other[byte] ^= (uint8_t)(1 << bit);
            CHECK_MSG(Crc32::Compute(other) != base, "byte " + std::to_string(byte) + " bit " + std::to_string(bit));
        }
    }
}

TEST(crc32_hex_helpers) {
    CHECK_EQ_STR(Crc32::ToHex(0), "00000000");
    CHECK_EQ_STR(Crc32::ToHex(0xCBF43926u), "cbf43926");
    CHECK_EQ_STR(Crc32::ToHex(0xFFFFFFFFu), "ffffffff");
    CHECK_EQ_STR(Crc32::ToHex(0x0000ABCDu), "0000abcd");

    uint32_t value = 0;
    CHECK(Crc32::FromHex("cbf43926", value));
    CHECK(value == 0xCBF43926u);
    CHECK(Crc32::FromHex("CBF43926", value));
    CHECK(value == 0xCBF43926u);
    CHECK(Crc32::FromHex("00000000", value));
    CHECK(value == 0u);

    uint32_t untouched = 0x12345678u;
    for (const char* bad : { "", "cbf4392", "cbf439260", "cbf4392g", "cbf4392 ", " cbf4392", "0xcbf4392", "cbf-3926" }) {
        value = untouched;
        CHECK_MSG(!Crc32::FromHex(bad, value), bad);
        CHECK_MSG(value == untouched, bad);
    }
}

TEST(cook_cipher_chacha12_matches_independent_implementation) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::ChaCha12);
    REQUIRE(cipher != nullptr);
    for (const vectors::CookCipherVector& v : vectors::kCookCipher) {
        std::vector<uint8_t> plain = Bytes(v.plain);
        std::vector<uint8_t> expected = Bytes(v.cipher);

        std::vector<uint8_t> data = plain;
        cipher->Encrypt(data, v.key, v.context);
        CHECK_MSG(test::BytesEqual(data, expected), std::string("context ") + v.context);

        cipher->Decrypt(data, v.key, v.context);
        CHECK_BYTES(data, plain);
    }
}

TEST(cook_cipher_chacha12_separates_keys_and_contexts) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::ChaCha12);
    REQUIRE(cipher != nullptr);
    std::vector<uint8_t> plain = Pattern(256, 21);

    auto encrypt = [&](const std::string& key, const std::string& context) {
        std::vector<uint8_t> data = plain;
        cipher->Encrypt(data, key, context);
        return data;
    };

    std::vector<uint8_t> base = encrypt("key-one", "file/a");
    CHECK(!test::BytesEqual(base, plain));
    CHECK_BYTES(encrypt("key-one", "file/a"), base);
    CHECK(!test::BytesEqual(encrypt("key-one", "file/b"), base));
    CHECK(!test::BytesEqual(encrypt("key-two", "file/a"), base));
    CHECK(!test::BytesEqual(encrypt("key-one", "file/A"), base));

    std::vector<uint8_t> wrongKey = base;
    cipher->Decrypt(wrongKey, "key-two", "file/a");
    CHECK(!test::BytesEqual(wrongKey, plain));

    std::vector<uint8_t> wrongContext = base;
    cipher->Decrypt(wrongContext, "key-one", "file/b");
    CHECK(!test::BytesEqual(wrongContext, plain));
}

TEST(cook_cipher_chacha12_encrypts_with_an_empty_key) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::ChaCha12);
    REQUIRE(cipher != nullptr);
    std::vector<uint8_t> plain = Pattern(100, 7);
    std::vector<uint8_t> data = plain;
    cipher->Encrypt(data, "", "ctx");
    CHECK(!test::BytesEqual(data, plain));
    cipher->Decrypt(data, "", "ctx");
    CHECK_BYTES(data, plain);
}

TEST(cook_cipher_handles_empty_data) {
    for (const std::string& id : { CookCiphers::None, CookCiphers::ChaCha12 }) {
        std::unique_ptr<ICookCipher> cipher = CreateCookCipher(id);
        REQUIRE(cipher != nullptr);
        std::vector<uint8_t> data;
        cipher->Encrypt(data, "key", "context");
        CHECK(data.empty());
        cipher->Decrypt(data, "key", "context");
        CHECK(data.empty());
    }
}

TEST(cook_cipher_none_leaves_data_untouched) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::None);
    REQUIRE(cipher != nullptr);
    std::vector<uint8_t> plain = Pattern(64, 41);
    std::vector<uint8_t> data = plain;
    cipher->Encrypt(data, "key", "ctx");
    CHECK_BYTES(data, plain);
    cipher->Decrypt(data, "key", "ctx");
    CHECK_BYTES(data, plain);
}

TEST(cook_cipher_lookup_rules) {
    CHECK(CreateCookCipher("DoesNotExist") == nullptr);
    CHECK(CreateCookCipher("XorSimple") == nullptr);
    CHECK(CreateCookCipher("Aes256Ctr") == nullptr);
    CHECK(CreateCookCipher("") != nullptr);

    CookCipherInfo info;
    CHECK(CookCipherRegistry::Get().Find(CookCiphers::None, info));
    CHECK(!info.usesKey);
    CHECK(CookCipherRegistry::Get().Find(CookCiphers::ChaCha12, info));
    CHECK(info.usesKey);
    CHECK(CookCipherRegistry::Get().IsAvailable(info));
    CHECK(!CookCipherRegistry::Get().Find("DoesNotExist", info));
}

TEST(cook_cipher_registry_rejects_incomplete_entries_and_replaces_by_id) {
    size_t before = CookCipherRegistry::Get().GetAll().size();

    CookCipherInfo noId;
    noId.create = []() { return CreateCookCipher(CookCiphers::None); };
    CookCipherRegistry::Get().Register(noId);

    CookCipherInfo noFactory;
    noFactory.id = "TestNoFactory";
    CookCipherRegistry::Get().Register(noFactory);

    CHECK(CookCipherRegistry::Get().GetAll().size() == before);

    CookCipherInfo custom;
    custom.id = "TestCustomCipher";
    custom.displayName = "first";
    custom.create = []() { return CreateCookCipher(CookCiphers::None); };
    CookCipherRegistry::Get().Register(custom);
    CHECK(CookCipherRegistry::Get().GetAll().size() == before + 1);

    custom.displayName = "second";
    CookCipherRegistry::Get().Register(custom);
    CHECK(CookCipherRegistry::Get().GetAll().size() == before + 1);

    CookCipherInfo found;
    CHECK(CookCipherRegistry::Get().Find("TestCustomCipher", found));
    CHECK_EQ_STR(found.displayName, "second");
}

TEST(cook_key_generation) {
    std::string a = GenerateCookKey(CookCiphers::ChaCha12);
    std::string b = GenerateCookKey(CookCiphers::ChaCha12);
    CHECK(a.size() == 64);
    CHECK(a != b);
    for (char c : a) CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));

    CHECK(GenerateCookKey(CookCiphers::None).empty());
    CHECK(GenerateCookKey("DoesNotExist").empty());
}

TEST(cook_key_generation_covers_the_whole_hex_alphabet) {
    bool seen[16] = {};
    for (int i = 0; i < 20; i++) {
        for (char c : GenerateCookKey(CookCiphers::ChaCha12)) {
            int value = (c >= '0' && c <= '9') ? c - '0' : c - 'a' + 10;
            seen[value] = true;
        }
    }
    for (int i = 0; i < 16; i++) CHECK_MSG(seen[i], "hex digit " + std::to_string(i));
}

TEST(cook_key_generation_fallback_for_custom_ciphers_is_alphanumeric) {
    CookCipherInfo custom;
    custom.id = "TestKeyedNoGenerator";
    custom.usesKey = true;
    custom.create = []() { return CreateCookCipher(CookCiphers::None); };
    CookCipherRegistry::Get().Register(custom);

    std::string a = GenerateCookKey("TestKeyedNoGenerator");
    std::string b = GenerateCookKey("TestKeyedNoGenerator");
    CHECK(a.size() == 24);
    CHECK(a != b);
    for (char c : a) CHECK(std::isalnum((unsigned char)c) != 0);
}

TEST_MAIN()
