#include "TestHelper.h"
#include "Core/CookedText.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

bool Contains(const std::vector<uint8_t>& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

std::string RoundTrip(const std::string& text, uint32_t seed, bool& ok) {
    std::vector<uint8_t> packed;
    ok = CookedText::Pack(text, seed, packed);
    if (!ok) return std::string();
    std::string out;
    ok = CookedText::Unpack(packed.data(), packed.size(), out);
    return out;
}

const char* kSample =
    "[Actor]\nVersion=3\nName=Player\n\n[Component0]\nType=Mesh\nSource=_Source/hero.glb\nMaterial0=Materials/Hero.starmat\n"
    "Health=100\nHealthRegen=2.5\n\n[Functions]\nfunction OnTick(dt) {\n    self.Health = self.Health + HealthRegen * dt\n}\n";

}

TEST(cooked_text_round_trips_ordinary_assets) {
    for (uint32_t seed : { 0u, 1u, 7u, 0xFFFFFFFFu, 0xDEADBEEFu }) {
        bool ok = false;
        std::string out = RoundTrip(kSample, seed, ok);
        CHECK_MSG(ok, "seed " + std::to_string(seed));
        CHECK_EQ_STR(out, kSample);
    }
}

TEST(cooked_text_round_trips_edge_cases) {
    const std::vector<std::string> cases = {
        "", "a", "\n", "\r\n\r\n", "word", "word word word", "_", "9", "a=b", "   ", "\t\t",
        "no trailing newline", "trailing newline\n", "\xC3\xBC\xD1\x84\xE3\x81\x82 unicode", std::string("nul\0inside", 10),
    };
    for (const std::string& text : cases) {
        bool ok = false;
        std::string out = RoundTrip(text, 42, ok);
        CHECK_MSG(ok, "case size " + std::to_string(text.size()));
        CHECK_MSG(out == text, "case size " + std::to_string(text.size()));
    }
}

TEST(cooked_text_round_trips_every_byte_value_and_random_text) {
    std::string all;
    for (int i = 0; i < 256; i++) all.push_back((char)i);
    bool ok = false;
    CHECK_EQ_STR(RoundTrip(all, 5, ok), all);
    CHECK(ok);

    test::Rng rng(99);
    for (int round = 0; round < 200; round++) {
        std::string text;
        const size_t length = rng.Below(400);
        for (size_t i = 0; i < length; i++) {
            const uint32_t pick = rng.Below(10);
            if (pick < 6) text.push_back((char)('a' + rng.Below(4)));
            else if (pick < 8) text.push_back(std::string("=\n \t_[]").at(rng.Below(7)));
            else text.push_back((char)rng.Next());
        }
        std::string out = RoundTrip(text, (uint32_t)rng.Next(), ok);
        CHECK_MSG(ok && out == text, "round " + std::to_string(round));
    }
}

TEST(cooked_text_is_deterministic_per_seed_and_differs_between_seeds) {
    std::vector<uint8_t> a, b, c;
    REQUIRE(CookedText::Pack(kSample, 11, a));
    REQUIRE(CookedText::Pack(kSample, 11, b));
    REQUIRE(CookedText::Pack(kSample, 12, c));
    CHECK_BYTES(a, b);
    CHECK(!test::BytesEqual(a, c));
    CHECK(CookedText::LooksCooked(a.data(), a.size()));
}

TEST(cooked_text_does_not_leave_identifiers_readable) {
    std::vector<uint8_t> packed;
    REQUIRE(CookedText::Pack(kSample, 1234, packed));
    for (const char* word : { "Health", "HealthRegen", "OnTick", "Component0", "Materials", "hero", "function", "Player", "Source" }) {
        CHECK_MSG(!Contains(packed, word), word);
    }
}

TEST(cooked_text_shrinks_repetitive_text) {
    std::string text;
    for (int i = 0; i < 200; i++) text += "InstanceName=Crate\nClass=Prop\nMesh=Meshes/Crate.starasset\n";
    std::vector<uint8_t> packed;
    REQUIRE(CookedText::Pack(text, 3, packed));
    CHECK(packed.size() < text.size() / 2);
}

TEST(cooked_text_unpack_rejects_every_truncation_and_trailing_bytes) {
    std::vector<uint8_t> packed;
    REQUIRE(CookedText::Pack(kSample, 77, packed));

    for (size_t length = 0; length < packed.size(); length++) {
        std::string out = "stale";
        const bool ok = CookedText::Unpack(packed.data(), length, out);
        if (!ok) CHECK(out.empty());
        else CHECK_MSG(false, "truncated to " + std::to_string(length) + " was accepted");
    }

    std::vector<uint8_t> extended = packed;
    extended.push_back(0);
    std::string out;
    CHECK(!CookedText::Unpack(extended.data(), extended.size(), out));
}

TEST(cooked_text_unpack_rejects_hostile_headers) {
    auto make = [](std::vector<uint8_t> body) {
        std::vector<uint8_t> out = { 'S', 'T', 'X', 'T', '1', 0, 0, 0, 0 };
        out.insert(out.end(), body.begin(), body.end());
        return out;
    };
    const std::vector<std::pair<const char*, std::vector<uint8_t>>> cases = {
        { "bad magic", { 'S', 'T', 'X', 'T', '2', 0, 0, 0, 0, 0, 0 } },
        { "empty", {} },
        { "only magic", { 'S', 'T', 'X', 'T', '1' } },
        { "size varint overlong", make({ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0 }) },
        { "size too large", make({ 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0 }) },
        { "word count too large", make({ 5, 0xFF, 0xFF, 0xFF, 0x0F }) },
        { "word count beyond data", make({ 5, 100 }) },
        { "word length zero", make({ 5, 1, 0 }) },
        { "word length beyond data", make({ 5, 1, 50, 'a' }) },
        { "word longer than output", make({ 2, 1, 5, 'a', 'b', 'c', 'd', 'e' }) },
        { "code references missing word", make({ 3, 0, 0x80, 0x02 }) },
        { "literal overflows output", make({ 1, 0, 1, 2, 3 }) },
        { "output shorter than declared", make({ 9, 0, 1 }) },
        { "code varint cut off", make({ 3, 0, 0x80 }) },
    };
    for (const auto& c : cases) {
        std::string out = "stale";
        CHECK_MSG(!CookedText::Unpack(c.second.data(), c.second.size(), out), c.first);
        CHECK_MSG(out.empty(), c.first);
    }
}

TEST(cooked_text_unpack_survives_random_damage) {
    std::vector<uint8_t> packed;
    REQUIRE(CookedText::Pack(kSample, 5, packed));
    test::Rng rng(4242);
    for (int round = 0; round < 4000; round++) {
        std::vector<uint8_t> damaged = packed;
        const int edits = 1 + (int)rng.Below(4);
        for (int e = 0; e < edits; e++) damaged[rng.Below((uint32_t)damaged.size())] = (uint8_t)rng.Next();
        if (rng.Below(5) == 0) damaged.resize(rng.Below((uint32_t)damaged.size()));
        std::string out;
        CookedText::Unpack(damaged.data(), damaged.size(), out);
    }

    for (int round = 0; round < 2000; round++) {
        std::vector<uint8_t> noise = { 'S', 'T', 'X', 'T', '1' };
        std::vector<uint8_t> tail = rng.Bytes(rng.Below(80));
        noise.insert(noise.end(), tail.begin(), tail.end());
        std::string out;
        CookedText::Unpack(noise.data(), noise.size(), out);
    }
}

TEST(cooked_text_pack_refuses_oversized_input) {
    std::vector<uint8_t> out = { 1 };
    CHECK(!CookedText::Pack(std::string(CookedText::kMaxTextBytes + 1, 'a'), 1, out));
    CHECK(out.empty());
}

TEST_MAIN()
