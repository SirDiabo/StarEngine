#include "TestHelper.h"
#include "Core/Aes256.h"
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

TEST(aes256_block_known_answers) {
    for (const vectors::AesBlockVector& v : vectors::kAesBlocks) {
        std::vector<uint8_t> key = Bytes(v.key);
        std::vector<uint8_t> plain = Bytes(v.plain);
        std::vector<uint8_t> expected = Bytes(v.cipher);
        REQUIRE(key.size() == 32 && plain.size() == 16 && expected.size() == 16);

        Aes256::Key expanded;
        Aes256::ExpandKey(key.data(), expanded);
        std::vector<uint8_t> out(16);
        Aes256::EncryptBlock(expanded, plain.data(), out.data());
        CHECK_BYTES(out, expected);
    }
}

TEST(aes256_encrypt_block_is_deterministic) {
    std::vector<uint8_t> key = Pattern(32, 4);
    std::vector<uint8_t> plain = Pattern(16, 5);
    Aes256::Key expanded;
    Aes256::ExpandKey(key.data(), expanded);
    std::vector<uint8_t> a(16), b(16);
    Aes256::EncryptBlock(expanded, plain.data(), a.data());
    Aes256::EncryptBlock(expanded, plain.data(), b.data());
    CHECK_BYTES(a, b);
    CHECK(!test::BytesEqual(a, plain));
}

TEST(aes256_ctr_known_answers_and_symmetry) {
    for (const vectors::AesCtrVector& v : vectors::kAesCtr) {
        std::vector<uint8_t> key = Bytes(v.key);
        std::vector<uint8_t> counter = Bytes(v.counter);
        std::vector<uint8_t> plain = Bytes(v.plain);
        std::vector<uint8_t> expected = Bytes(v.cipher);
        REQUIRE(key.size() == 32 && counter.size() == 16 && plain.size() == expected.size());

        Aes256::Key expanded;
        Aes256::ExpandKey(key.data(), expanded);

        std::vector<uint8_t> data = plain;
        Aes256::CtrXor(expanded, counter.data(), data.data(), data.size());
        CHECK_BYTES(data, expected);

        Aes256::CtrXor(expanded, counter.data(), data.data(), data.size());
        CHECK_BYTES(data, plain);
    }
}

TEST(aes256_ctr_keystream_is_prefix_stable) {
    std::vector<uint8_t> key = Pattern(32, 8);
    std::vector<uint8_t> counter = Bytes("00000000000000000000000000ffffff");
    Aes256::Key expanded;
    Aes256::ExpandKey(key.data(), expanded);

    std::vector<uint8_t> zeros(200, 0);
    std::vector<uint8_t> full = zeros;
    Aes256::CtrXor(expanded, counter.data(), full.data(), full.size());

    for (size_t length = 0; length <= 100; length++) {
        std::vector<uint8_t> prefix(length, 0);
        Aes256::CtrXor(expanded, counter.data(), prefix.data(), prefix.size());
        std::vector<uint8_t> expectedPrefix(full.begin(), full.begin() + (std::ptrdiff_t)length);
        CHECK_MSG(test::BytesEqual(prefix, expectedPrefix), "length " + std::to_string(length));
    }
}

TEST(aes256_ctr_zero_length_is_a_no_op) {
    std::vector<uint8_t> key = Pattern(32, 9);
    std::vector<uint8_t> counter(16, 0);
    Aes256::Key expanded;
    Aes256::ExpandKey(key.data(), expanded);
    Aes256::CtrXor(expanded, counter.data(), nullptr, 0);
    std::vector<uint8_t> untouched = Pattern(5, 1);
    std::vector<uint8_t> copy = untouched;
    Aes256::CtrXor(expanded, counter.data(), copy.data(), 0);
    CHECK_BYTES(copy, untouched);
}

TEST(aes256_ctr_counter_wraps_across_all_bytes) {
    std::vector<uint8_t> key = Pattern(32, 13);
    Aes256::Key expanded;
    Aes256::ExpandKey(key.data(), expanded);

    std::vector<uint8_t> allOnes(16, 0xFF);
    std::vector<uint8_t> zero(16, 0);

    std::vector<uint8_t> twoBlocks(32, 0);
    Aes256::CtrXor(expanded, allOnes.data(), twoBlocks.data(), twoBlocks.size());

    std::vector<uint8_t> first(16, 0), second(16, 0);
    Aes256::CtrXor(expanded, allOnes.data(), first.data(), first.size());
    Aes256::CtrXor(expanded, zero.data(), second.data(), second.size());

    CHECK_BYTES(std::vector<uint8_t>(twoBlocks.begin(), twoBlocks.begin() + 16), first);
    CHECK_BYTES(std::vector<uint8_t>(twoBlocks.begin() + 16, twoBlocks.end()), second);
}

TEST(cook_cipher_aes_matches_independent_implementation) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::Aes256Ctr);
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

TEST(cook_cipher_aes_separates_keys_and_contexts) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::Aes256Ctr);
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

TEST(cook_cipher_handles_empty_data) {
    for (const std::string& id : { CookCiphers::None, CookCiphers::XorSimple, CookCiphers::Aes256Ctr }) {
        std::unique_ptr<ICookCipher> cipher = CreateCookCipher(id);
        REQUIRE(cipher != nullptr);
        std::vector<uint8_t> data;
        cipher->Encrypt(data, "key", "context");
        CHECK(data.empty());
        cipher->Decrypt(data, "key", "context");
        CHECK(data.empty());
    }
}

TEST(cook_cipher_xor_round_trips) {
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(CookCiphers::XorSimple);
    REQUIRE(cipher != nullptr);
    std::vector<uint8_t> plain = Pattern(1000, 31);
    std::vector<uint8_t> data = plain;
    cipher->Encrypt(data, "AbCdEfGh", "ignored");
    CHECK(!test::BytesEqual(data, plain));
    cipher->Decrypt(data, "AbCdEfGh", "different context is ignored");
    CHECK_BYTES(data, plain);

    std::vector<uint8_t> emptyKey = plain;
    cipher->Encrypt(emptyKey, "", "ctx");
    CHECK_BYTES(emptyKey, plain);
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
    CHECK(CreateCookCipher("") != nullptr);

    CookCipherInfo info;
    CHECK(CookCipherRegistry::Get().Find(CookCiphers::None, info));
    CHECK(!info.usesKey);
    CHECK(CookCipherRegistry::Get().Find(CookCiphers::XorSimple, info));
    CHECK(info.usesKey);
    CHECK(CookCipherRegistry::Get().Find(CookCiphers::Aes256Ctr, info));
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
    std::string aesA = GenerateCookKey(CookCiphers::Aes256Ctr);
    std::string aesB = GenerateCookKey(CookCiphers::Aes256Ctr);
    CHECK(aesA.size() == 64);
    CHECK(aesA != aesB);
    for (char c : aesA) CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));

    std::string xorA = GenerateCookKey(CookCiphers::XorSimple);
    std::string xorB = GenerateCookKey(CookCiphers::XorSimple);
    CHECK(xorA.size() == 24);
    CHECK(xorA != xorB);
    for (char c : xorA) CHECK(std::isalnum((unsigned char)c) != 0);

    CHECK(GenerateCookKey(CookCiphers::None).empty());
    CHECK(GenerateCookKey("DoesNotExist").empty());
}

TEST_MAIN()
