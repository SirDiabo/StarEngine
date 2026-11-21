#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/CookedEntry.h"
#include "Core/PackageCipher.h"
#include "Core/PackageCompression.h"
#include "Core/StarPak.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using test::TempDir;

std::vector<uint8_t> Pattern(size_t count, unsigned seed) {
    std::vector<uint8_t> out(count);
    for (size_t i = 0; i < count; i++) out[i] = (uint8_t)(((i / 3) * 31 + seed) & 0xFF);
    return out;
}

struct Source {
    std::string path;
    std::vector<uint8_t> bytes;
    std::string compression;
};

std::vector<Source> Sources() {
    test::Rng rng(21);
    return {
        { "Content/a.txt", Pattern(500, 1), PackCompressions::None },
        { "Content/b.bin", rng.Bytes(3000), PackCompressions::None },
        { "Content/c.starmap", Pattern(20000, 5), PackCompressions::Deflate },
        { "Content/d.png", rng.Bytes(900), PackCompressions::Deflate },
        { "Content/empty.bin", {}, PackCompressions::None },
        { "__boot__", Pattern(60, 9), PackCompressions::None },
    };
}

bool AddCooked(StarPak::Writer& writer, const Source& s, const std::string& cipherId, const std::string& key) {
    std::vector<uint8_t> stored = s.bytes;
    uint64_t rawSize = s.bytes.size();
    std::string compression = StarPak::kNoCompression;
    if (s.compression != PackCompressions::None && !s.bytes.empty()) {
        std::vector<uint8_t> packed;
        if (!PackageCompression::Compress(s.compression, s.bytes, packed)) return false;
        stored = packed;
        compression = s.compression;
    }
    std::unique_ptr<ICookCipher> cipher = CreateCookCipher(cipherId);
    cipher->Encrypt(stored, key, s.path);
    return writer.AddEntry(s.path, stored, rawSize, compression);
}

std::vector<uint8_t> BuildPak(const std::string& cipherId, const std::string& key, const TempDir& dir) {
    StarPak::Writer writer;
    writer.SetIndexKey(key);
    for (const Source& s : Sources()) {
        if (!AddCooked(writer, s, cipherId, key)) return {};
    }
    const std::string file = dir.Str("cooked.pak");
    if (!writer.WriteToFile(file)) return {};
    return test::ReadFile(file);
}

}

TEST(cooked_entry_reads_encrypted_and_compressed_entries) {
    TempDir dir;
    const std::string key = "entry test key";
    std::vector<uint8_t> bytes = BuildPak(CookCiphers::ChaCha12, key, dir);
    REQUIRE(!bytes.empty());

    StarPak::Reader pak;
    REQUIRE(pak.LoadBuffer(bytes, key));
    for (const Source& s : Sources()) {
        std::vector<uint8_t> out = { 1, 2, 3 };
        std::string error;
        CHECK_MSG(CookedEntry::Read(pak, CookCiphers::ChaCha12, key, s.path, out, error), s.path + ": " + error);
        CHECK_MSG(test::BytesEqual(out, s.bytes), s.path);
    }
}

TEST(cooked_entry_stores_nothing_readable) {
    TempDir dir;
    std::vector<uint8_t> bytes = BuildPak(CookCiphers::ChaCha12, "k", dir);
    const std::vector<uint8_t> original = Sources()[0].bytes;
    CHECK(std::search(bytes.begin(), bytes.end(), original.begin(), original.begin() + 64) == bytes.end());
}

TEST(cooked_entry_uses_a_different_keystream_per_path) {
    StarPak::Writer writer;
    writer.SetIndexKey("k");
    Source one{ "Content/one.bin", Pattern(400, 3), PackCompressions::None };
    Source two{ "Content/two.bin", Pattern(400, 3), PackCompressions::None };
    REQUIRE(AddCooked(writer, one, CookCiphers::ChaCha12, "k"));
    REQUIRE(AddCooked(writer, two, CookCiphers::ChaCha12, "k"));
    TempDir dir;
    REQUIRE(writer.WriteToFile(dir.Str("two.pak")));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("two.pak"), "k"));
    std::vector<uint8_t> a = pak.ReadEntry("Content/one.bin");
    std::vector<uint8_t> b = pak.ReadEntry("Content/two.bin");
    CHECK(a.size() == 400 && b.size() == 400);
    CHECK(!test::BytesEqual(a, b));
}

TEST(cooked_entry_wrong_key_never_yields_the_original) {
    TempDir dir;
    std::vector<uint8_t> bytes = BuildPak(CookCiphers::ChaCha12, "right", dir);
    StarPak::Reader pak;
    REQUIRE(pak.LoadBuffer(bytes, "right"));

    for (const Source& s : Sources()) {
        if (s.bytes.empty()) continue;
        std::vector<uint8_t> out;
        std::string error;
        const bool ok = CookedEntry::Read(pak, CookCiphers::ChaCha12, "wrong", s.path, out, error);
        if (ok) CHECK_MSG(!test::BytesEqual(out, s.bytes), s.path);
        else CHECK_MSG(out.empty(), s.path);
    }
}

TEST(cooked_entry_reports_missing_entries_and_unknown_methods) {
    TempDir dir;
    std::vector<uint8_t> bytes = BuildPak(CookCiphers::ChaCha12, "k", dir);
    StarPak::Reader pak;
    REQUIRE(pak.LoadBuffer(bytes, "k"));

    std::vector<uint8_t> out = { 9 };
    std::string error;
    CHECK(!CookedEntry::Read(pak, CookCiphers::ChaCha12, "k", "Content/missing.bin", out, error));
    CHECK(out.empty());
    CHECK(error.find("not found") != std::string::npos);

    CHECK(!CookedEntry::Read(pak, "NoSuchCipher", "k", "Content/a.txt", out, error));
    CHECK(out.empty());
    CHECK(error.find("NoSuchCipher") != std::string::npos);

    StarPak::Writer writer;
    writer.SetIndexKey("k");
    REQUIRE(writer.AddEntry("x", { 1, 2, 3 }, 9, "MadeUp"));
    REQUIRE(writer.WriteToFile(dir.Str("made.pak")));
    StarPak::Reader madeUp;
    REQUIRE(madeUp.LoadFile(dir.Str("made.pak"), "k"));
    CHECK(!CookedEntry::Read(madeUp, CookCiphers::None, "", "x", out, error));
    CHECK(out.empty());
    CHECK(error.find("decompress") != std::string::npos);
}

TEST(cooked_entry_detects_damaged_stored_bytes) {
    TempDir dir;
    const std::string key = "k";
    std::vector<uint8_t> bytes = BuildPak(CookCiphers::ChaCha12, key, dir);

    StarPak::Reader clean;
    REQUIRE(clean.LoadBuffer(bytes, key));
    StarPak::Entry info;
    REQUIRE(clean.GetEntryInfo("Content/b.bin", info));

    size_t headerEnd = 9;
    while (bytes[headerEnd] != '\n') headerEnd++;
    headerEnd++;
    size_t indexSize = 0;
    for (size_t i = 9; bytes[i] != '\t'; i++) indexSize = indexSize * 10 + (size_t)(bytes[i] - '0');
    const size_t blobStart = headerEnd + indexSize;

    std::vector<uint8_t> damaged = bytes;
    damaged[blobStart + info.offset + 10] ^= 0x04;
    StarPak::Reader pak;
    REQUIRE(pak.LoadBuffer(damaged, key));

    std::vector<uint8_t> out;
    std::string error;
    CHECK(!CookedEntry::Read(pak, CookCiphers::ChaCha12, key, "Content/b.bin", out, error));
    CHECK(out.empty());
    CHECK(error.find("crc32") != std::string::npos);
    CHECK(CookedEntry::Read(pak, CookCiphers::ChaCha12, key, "Content/a.txt", out, error));
}

TEST(cooked_entry_rejects_inflated_data_of_the_wrong_size) {
    TempDir dir;
    std::vector<uint8_t> raw = Pattern(5000, 4);
    std::vector<uint8_t> packed;
    REQUIRE(PackageCompression::Compress(PackCompressions::Deflate, raw, packed));

    StarPak::Writer writer;
    writer.SetIndexKey("k");
    REQUIRE(writer.AddEntry("short", packed, raw.size() - 1, PackCompressions::Deflate));
    REQUIRE(writer.AddEntry("long", packed, raw.size() + 1, PackCompressions::Deflate));
    REQUIRE(writer.AddEntry("exact", packed, raw.size(), PackCompressions::Deflate));
    REQUIRE(writer.WriteToFile(dir.Str("sizes.pak")));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("sizes.pak"), "k"));
    std::vector<uint8_t> out;
    std::string error;
    CHECK(!CookedEntry::Read(pak, CookCiphers::None, "", "short", out, error));
    CHECK(!CookedEntry::Read(pak, CookCiphers::None, "", "long", out, error));
    CHECK(CookedEntry::Read(pak, CookCiphers::None, "", "exact", out, error));
    CHECK_BYTES(out, raw);
}

TEST_MAIN()
