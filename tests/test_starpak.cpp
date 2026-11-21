#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/StarPak.h"
#include "Core/Crc32.h"
#include "Core/Sha256.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using test::TempDir;

std::vector<uint8_t> Pattern(size_t count, unsigned seed) {
    std::vector<uint8_t> out(count);
    for (size_t i = 0; i < count; i++) out[i] = (uint8_t)((i * 31 + seed) & 0xFF);
    return out;
}

std::vector<uint8_t> ToBytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

struct Sample {
    std::string path;
    std::vector<uint8_t> bytes;
};

std::vector<Sample> StandardSamples() {
    test::Rng rng(5);
    return {
        { "Content/Textures/wall.png", rng.Bytes(1000) },
        { "Content/Empty.bin", {} },
        { "Content/Levels/Main.starmap", ToBytes("Level=Main\nActors=3\n") },
        { "__boot__", ToBytes("boot config") },
        { "Content/Name With Spaces.txt", Pattern(77, 3) },
        { "Content/Unicode/\xC3\xBC\xD1\x84\xE3\x81\x82.dat", Pattern(300, 9) },
        { "Content/Big.bin", rng.Bytes(100000) },
        { "Content/Zeros.bin", std::vector<uint8_t>(4096, 0) },
    };
}

std::vector<uint8_t> BuildPakBytes(const std::vector<Sample>& samples, const TempDir& dir) {
    StarPak::Writer writer;
    for (const Sample& s : samples) writer.AddFile(s.path, s.bytes);
    std::string file = dir.Str("build.pak");
    writer.WriteToFile(file);
    return test::ReadFile(file);
}

struct FooterBytes {
    char magic[8];
    uint64_t offset;
    uint64_t size;
};

std::vector<uint8_t> MakeFooter(uint64_t offset, uint64_t size, const char* magic = "STARPAKF") {
    FooterBytes footer{};
    std::memcpy(footer.magic, magic, 8);
    footer.offset = offset;
    footer.size = size;
    std::vector<uint8_t> out(sizeof(footer));
    std::memcpy(out.data(), &footer, sizeof(footer));
    return out;
}

std::vector<uint8_t> Concat(std::initializer_list<std::vector<uint8_t>> parts) {
    std::vector<uint8_t> out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

bool LoadText(StarPak::Reader& reader, const std::string& text) {
    return reader.LoadBuffer(ToBytes(text));
}

void CheckReaderBounded(const StarPak::Reader& reader, const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        StarPak::Entry info;
        if (!reader.GetEntryInfo(path, info)) continue;
        std::vector<uint8_t> out;
        std::string error;
        bool ok = reader.ReadEntryVerified(path, out, error);
        if (ok) {
            CHECK_MSG(out.size() == info.size, "verified read size");
        } else {
            CHECK_MSG(out.empty(), "failed read leaves no bytes");
            CHECK_MSG(!error.empty(), "failed read gives a reason");
        }
    }
}

}

TEST(starpak_round_trip_through_file) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();

    StarPak::Writer writer;
    for (const Sample& s : samples) CHECK_MSG(writer.AddFile(s.path, s.bytes), s.path);
    REQUIRE(writer.WriteToFile(dir.Str("game.pak")));

    StarPak::Reader reader;
    REQUIRE(reader.LoadFile(dir.Str("game.pak")));

    for (const Sample& s : samples) {
        CHECK_MSG(reader.HasEntry(s.path), s.path);
        StarPak::Entry info;
        REQUIRE(reader.GetEntryInfo(s.path, info));
        CHECK(info.path == s.path);
        CHECK(info.size == s.bytes.size());
        CHECK(info.rawSize == s.bytes.size());
        CHECK(info.compression == StarPak::kNoCompression);
        CHECK(info.sha256.empty());
        CHECK(info.hasCrc32);
        CHECK(info.crc32 == Crc32::Compute(s.bytes));

        std::vector<uint8_t> out;
        std::string error;
        CHECK_MSG(reader.ReadEntryVerified(s.path, out, error), s.path + ": " + error);
        CHECK_MSG(test::BytesEqual(out, s.bytes), s.path);
        CHECK_MSG(test::BytesEqual(reader.ReadEntry(s.path), s.bytes), s.path);
    }

    CHECK(!reader.HasEntry("Content/Missing.png"));
    StarPak::Entry info;
    CHECK(!reader.GetEntryInfo("Content/Missing.png", info));
    std::vector<uint8_t> out = { 1 };
    std::string error;
    CHECK(!reader.ReadEntryVerified("Content/Missing.png", out, error));
    CHECK(out.empty());
    CHECK_EQ_STR(error, "entry not found");
    CHECK(reader.ReadEntry("Content/Missing.png").empty());
}

TEST(starpak_load_buffer_matches_load_file) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();
    std::vector<uint8_t> bytes = BuildPakBytes(samples, dir);

    StarPak::Reader fromBuffer;
    REQUIRE(fromBuffer.LoadBuffer(bytes));
    StarPak::Reader fromFile;
    REQUIRE(fromFile.LoadFile(dir.Str("build.pak")));
    for (const Sample& s : samples) {
        CHECK_BYTES(fromBuffer.ReadEntry(s.path), fromFile.ReadEntry(s.path));
        CHECK_BYTES(fromBuffer.ReadEntry(s.path), s.bytes);
    }
}

TEST(starpak_writer_output_is_deterministic) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();
    std::vector<uint8_t> first = BuildPakBytes(samples, dir);
    std::vector<uint8_t> second = BuildPakBytes(samples, dir);
    CHECK_BYTES(first, second);
}

TEST(starpak_on_disk_format_is_stable) {
    TempDir dir;
    StarPak::Writer writer;
    REQUIRE(writer.AddFile("a.txt", ToBytes("hello")));
    std::vector<uint8_t> ten(10);
    for (int i = 0; i < 10; i++) ten[(size_t)i] = (uint8_t)i;
    REQUIRE(writer.AddEntry("dir/b.bin", ten, 40, "Deflate"));
    REQUIRE(writer.WriteToFile(dir.Str("golden.pak")));

    std::string expected =
        "STARPAK3\n2\n"
        "a.txt\t0\t5\t5\tNone\t3610a686\n"
        "dir/b.bin\t5\t10\t40\tDeflate\t456cd746\n";
    std::vector<uint8_t> expectedBytes = ToBytes(expected);
    expectedBytes.insert(expectedBytes.end(), { 'h', 'e', 'l', 'l', 'o' });
    expectedBytes.insert(expectedBytes.end(), ten.begin(), ten.end());
    CHECK_BYTES(test::ReadFile(dir.Path("golden.pak")), expectedBytes);
}

TEST(starpak_writer_always_records_crc32_in_the_hash_field) {
    TempDir dir;
    StarPak::Writer writer;
    REQUIRE(writer.AddFile("a.txt", ToBytes("hello")));
    REQUIRE(writer.AddFile("empty.bin", {}));
    REQUIRE(writer.WriteToFile(dir.Str("crc.pak")));

    std::vector<uint8_t> bytes = test::ReadFile(dir.Path("crc.pak"));
    std::string text(bytes.begin(), bytes.end());
    CHECK(text.find("a.txt\t0\t5\t5\tNone\t3610a686\n") != std::string::npos);
    CHECK(text.find("empty.bin\t5\t0\t0\tNone\t00000000\n") != std::string::npos);

    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(bytes));
    StarPak::Entry info;
    REQUIRE(reader.GetEntryInfo("a.txt", info));
    CHECK(info.hasCrc32);
    CHECK(info.crc32 == 0x3610A686u);
    CHECK(info.sha256.empty());
    REQUIRE(reader.GetEntryInfo("empty.bin", info));
    CHECK(info.hasCrc32);
    CHECK(info.crc32 == 0u);
}

TEST(starpak_detects_payload_tampering) {
    TempDir dir;
    std::vector<Sample> samples = {
        { "first.bin", Pattern(100, 1) },
        { "second.bin", Pattern(200, 2) },
        { "third.bin", Pattern(50, 3) },
    };
    std::vector<uint8_t> bytes = BuildPakBytes(samples, dir);
    size_t blobStart = bytes.size() - (100 + 200 + 50);

    std::vector<uint8_t> tampered = bytes;
    tampered[blobStart + 100 + 5] ^= 0x01;
    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(tampered));

    std::vector<uint8_t> out;
    std::string error;
    CHECK(reader.ReadEntryVerified("first.bin", out, error));
    CHECK(!reader.ReadEntryVerified("second.bin", out, error));
    CHECK(out.empty());
    CHECK(error.find("crc32") != std::string::npos);
    CHECK(reader.ReadEntryVerified("third.bin", out, error));
    CHECK(reader.ReadEntry("second.bin").empty());

    reader.SetVerifyOnRead(false);
    CHECK(reader.ReadEntryVerified("second.bin", out, error));
    CHECK(out.size() == 200);
    CHECK(!test::BytesEqual(out, samples[1].bytes));

    reader.SetVerifyOnRead(true);
    std::vector<uint8_t> lastByte = bytes;
    lastByte.back() ^= 0x80;
    StarPak::Reader reader2;
    REQUIRE(reader2.LoadBuffer(lastByte));
    CHECK(!reader2.ReadEntryVerified("third.bin", out, error));
    CHECK(reader2.ReadEntryVerified("first.bin", out, error));
}

TEST(starpak_detects_truncated_entries_through_crc32) {
    TempDir dir;
    std::vector<Sample> samples = {
        { "first.bin", Pattern(100, 1) },
        { "second.bin", Pattern(200, 2) },
    };
    std::vector<uint8_t> bytes = BuildPakBytes(samples, dir);
    std::vector<uint8_t> shifted = bytes;
    size_t blobStart = bytes.size() - 300;
    shifted.erase(shifted.begin() + (std::ptrdiff_t)blobStart);
    shifted.push_back(0);

    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(shifted));
    std::vector<uint8_t> out;
    std::string error;
    CHECK(!reader.ReadEntryVerified("first.bin", out, error));
    CHECK(!reader.ReadEntryVerified("second.bin", out, error));
    CHECK(error.find("crc32") != std::string::npos);
}

TEST(starpak_entry_bounds_are_enforced) {
    std::string header =
        "STARPAK3\n4\n"
        "ok\t0\t4\t4\tNone\n"
        "pastEnd\t2\t10\t10\tNone\n"
        "hugeOffset\t18446744073709551615\t2\t2\tNone\n"
        "hugeSize\t1\t18446744073709551615\t1\tNone\n";
    std::vector<uint8_t> bytes = ToBytes(header);
    bytes.insert(bytes.end(), { 1, 2, 3, 4 });

    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(bytes));

    std::vector<uint8_t> out;
    std::string error;
    CHECK(reader.ReadEntryVerified("ok", out, error));
    CHECK(out.size() == 4);
    for (const char* name : { "pastEnd", "hugeOffset", "hugeSize" }) {
        out = { 1 };
        CHECK_MSG(!reader.ReadEntryVerified(name, out, error), name);
        CHECK(out.empty());
        CHECK_MSG(error.find("outside") != std::string::npos, name);
        CHECK(reader.ReadEntry(name).empty());
    }
}

TEST(starpak_writer_rejects_invalid_virtual_paths) {
    TempDir dir;
    StarPak::Writer writer;
    std::vector<uint8_t> data = { 1, 2, 3 };
    CHECK(!writer.AddFile("", data));
    CHECK(!writer.AddFile("a\tb", data));
    CHECK(!writer.AddFile("a\nb", data));
    CHECK(!writer.AddFile("a\rb", data));
    CHECK(!writer.AddFile(std::string("a\0b", 3), data));
    CHECK(!writer.AddEntry("bad\tpath", data, 3, "None"));
    CHECK(writer.AddFile("good/path.bin", data));
    CHECK(writer.AddFile("../looks/like/traversal", data));
    REQUIRE(writer.WriteToFile(dir.Str("paths.pak")));

    StarPak::Reader reader;
    REQUIRE(reader.LoadFile(dir.Str("paths.pak")));
    CHECK(reader.HasEntry("good/path.bin"));
    CHECK(reader.HasEntry("../looks/like/traversal"));
    CHECK(!reader.HasEntry(""));
    CHECK(!reader.HasEntry("a"));
}

TEST(starpak_keeps_compression_metadata) {
    TempDir dir;
    StarPak::Writer writer;
    std::vector<uint8_t> stored = Pattern(30, 4);
    REQUIRE(writer.AddEntry("packed.bin", stored, 1000, "Deflate"));
    REQUIRE(writer.AddEntry("defaulted.bin", stored, 30, ""));
    REQUIRE(writer.WriteToFile(dir.Str("c.pak")));

    StarPak::Reader reader;
    REQUIRE(reader.LoadFile(dir.Str("c.pak")));
    StarPak::Entry packed, defaulted;
    REQUIRE(reader.GetEntryInfo("packed.bin", packed));
    REQUIRE(reader.GetEntryInfo("defaulted.bin", defaulted));
    CHECK(packed.compression == "Deflate");
    CHECK(packed.rawSize == 1000);
    CHECK(packed.size == 30);
    CHECK(defaulted.compression == StarPak::kNoCompression);
    CHECK_BYTES(reader.ReadEntry("packed.bin"), stored);
}

TEST(starpak_writer_creates_missing_directories) {
    TempDir dir;
    StarPak::Writer writer;
    REQUIRE(writer.AddFile("x", { 1 }));
    CHECK(writer.WriteToFile(dir.Str("deep/er/still/out.pak")));
    StarPak::Reader reader;
    CHECK(reader.LoadFile(dir.Str("deep/er/still/out.pak")));
}

TEST(starpak_empty_writer_produces_loadable_pak) {
    TempDir dir;
    StarPak::Writer writer;
    REQUIRE(writer.WriteToFile(dir.Str("empty.pak")));
    StarPak::Reader reader;
    CHECK(reader.LoadFile(dir.Str("empty.pak")));
    CHECK(!reader.HasEntry("anything"));
}

TEST(starpak_many_entries) {
    TempDir dir;
    StarPak::Writer writer;
    for (int i = 0; i < 5000; i++) {
        std::vector<uint8_t> data = { (uint8_t)i, (uint8_t)(i >> 8) };
        REQUIRE(writer.AddFile("Content/File" + std::to_string(i) + ".bin", data));
    }
    REQUIRE(writer.WriteToFile(dir.Str("many.pak")));
    StarPak::Reader reader;
    REQUIRE(reader.LoadFile(dir.Str("many.pak")));
    for (int i : { 0, 1, 255, 256, 4999 }) {
        std::vector<uint8_t> expected = { (uint8_t)i, (uint8_t)(i >> 8) };
        CHECK_BYTES(reader.ReadEntry("Content/File" + std::to_string(i) + ".bin"), expected);
    }
}

TEST(starpak_load_file_failure_cases) {
    TempDir dir;
    StarPak::Reader reader;
    CHECK(!reader.LoadFile(dir.Str("does_not_exist.pak")));
    REQUIRE(test::WriteFile(dir.Path("zero.pak"), {}));
    CHECK(!reader.LoadFile(dir.Str("zero.pak")));
    CHECK(!reader.LoadFile(dir.path.string()));
    REQUIRE(test::WriteText(dir.Path("text.pak"), "this is not a pak file at all"));
    CHECK(!reader.LoadFile(dir.Str("text.pak")));
}

TEST(starpak_reads_legacy_version_one) {
    std::vector<uint8_t> bytes = ToBytes("STARPAK1\n2\nfile1\t0\t3\nfile2\t3\t2\n");
    bytes.insert(bytes.end(), { 10, 11, 12, 20, 21 });
    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(bytes));
    StarPak::Entry info;
    REQUIRE(reader.GetEntryInfo("file2", info));
    CHECK(info.size == 2);
    CHECK(info.rawSize == 2);
    CHECK(info.compression == StarPak::kNoCompression);
    CHECK_BYTES(reader.ReadEntry("file1"), std::vector<uint8_t>({ 10, 11, 12 }));
    CHECK_BYTES(reader.ReadEntry("file2"), std::vector<uint8_t>({ 20, 21 }));
}

TEST(starpak_reads_legacy_version_two) {
    std::vector<uint8_t> bytes = ToBytes("STARPAK2\n2\na\t0\t3\t3\t0\nb\t3\t5\t100\t1\n");
    bytes.insert(bytes.end(), { 1, 2, 3, 4, 5, 6, 7, 8 });
    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(bytes));
    StarPak::Entry a, b;
    REQUIRE(reader.GetEntryInfo("a", a));
    REQUIRE(reader.GetEntryInfo("b", b));
    CHECK(a.compression == StarPak::kNoCompression);
    CHECK(b.compression == "Deflate");
    CHECK(b.rawSize == 100);

    StarPak::Reader unknownMethod;
    CHECK(!LoadText(unknownMethod, "STARPAK2\n1\na\t0\t1\t1\t2\nx"));
}

TEST(starpak_reads_current_version_with_and_without_hash) {
    std::string hash = Sha256::ToHex(Sha256::Compute(std::string("x")));
    std::string crc = Crc32::ToHex(Crc32::Compute(std::string("x")));

    StarPak::Reader withSha;
    CHECK(LoadText(withSha, "STARPAK3\n1\na\t0\t1\t1\tNone\t" + hash + "\nx"));
    CHECK_BYTES(withSha.ReadEntry("a"), std::vector<uint8_t>({ 'x' }));
    StarPak::Entry info;
    REQUIRE(withSha.GetEntryInfo("a", info));
    CHECK(info.sha256 == hash);
    CHECK(!info.hasCrc32);

    StarPak::Reader withCrc;
    CHECK(LoadText(withCrc, "STARPAK3\n1\na\t0\t1\t1\tNone\t" + crc + "\nx"));
    CHECK_BYTES(withCrc.ReadEntry("a"), std::vector<uint8_t>({ 'x' }));
    REQUIRE(withCrc.GetEntryInfo("a", info));
    CHECK(info.sha256.empty());
    CHECK(info.hasCrc32);
    CHECK(info.crc32 == Crc32::Compute(std::string("x")));

    std::string upper = crc;
    for (char& c : upper) if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
    StarPak::Reader upperCrc;
    CHECK(LoadText(upperCrc, "STARPAK3\n1\na\t0\t1\t1\tNone\t" + upper + "\nx"));
    CHECK_BYTES(upperCrc.ReadEntry("a"), std::vector<uint8_t>({ 'x' }));

    StarPak::Reader withoutHash;
    CHECK(LoadText(withoutHash, "STARPAK3\n1\na\t0\t1\t1\tNone\nx"));
    CHECK_BYTES(withoutHash.ReadEntry("a"), std::vector<uint8_t>({ 'x' }));

    StarPak::Reader emptyHashField;
    CHECK(LoadText(emptyHashField, "STARPAK3\n1\na\t0\t1\t1\tNone\t\nx"));
    CHECK_BYTES(emptyHashField.ReadEntry("a"), std::vector<uint8_t>({ 'x' }));

    StarPak::Reader wrongSha;
    REQUIRE(LoadText(wrongSha, "STARPAK3\n1\na\t0\t1\t1\tNone\t" + std::string(64, '0') + "\nx"));
    std::vector<uint8_t> out = { 1 };
    std::string error;
    CHECK(!wrongSha.ReadEntryVerified("a", out, error));
    CHECK(out.empty());
    CHECK(error.find("sha256") != std::string::npos);
    CHECK(wrongSha.ReadEntry("a").empty());

    StarPak::Reader wrongCrc;
    REQUIRE(LoadText(wrongCrc, "STARPAK3\n1\na\t0\t1\t1\tNone\t00000000\nx"));
    out = { 1 };
    CHECK(!wrongCrc.ReadEntryVerified("a", out, error));
    CHECK(out.empty());
    CHECK(error.find("crc32") != std::string::npos);
    CHECK(wrongCrc.ReadEntry("a").empty());

    wrongCrc.SetVerifyOnRead(false);
    CHECK(wrongCrc.ReadEntryVerified("a", out, error));
}

TEST(starpak_rejects_malformed_buffers) {
    std::string shortHex(63, 'a');
    std::string badHex(64, 'z');
    std::vector<std::pair<std::string, std::string>> cases = {
        { "empty", "" },
        { "short magic", "STARPAK" },
        { "wrong magic", "NOTAPAKX\n0\n" },
        { "future version", "STARPAK9\n0\n" },
        { "magic only", "STARPAK3" },
        { "magic and newline", "STARPAK3\n" },
        { "count not a number", "STARPAK3\nabc\n" },
        { "count negative", "STARPAK3\n-1\n" },
        { "count too large", "STARPAK3\n99999999\n" },
        { "count max", "STARPAK3\n18446744073709551615\n" },
        { "count out of range", "STARPAK3\n99999999999999999999999\n" },
        { "missing entries", "STARPAK3\n1\n" },
        { "second entry missing", "STARPAK3\n2\na\t0\t1\t1\tNone\nx" },
        { "empty path", "STARPAK3\n1\n\t0\t1\t1\tNone\nx" },
        { "missing compression field", "STARPAK3\n1\na\t0\t1\t1\nx" },
        { "empty compression", "STARPAK3\n1\na\t0\t1\t1\t\nx" },
        { "offset not numeric", "STARPAK3\n1\na\tx\t1\t1\tNone\nx" },
        { "size not numeric", "STARPAK3\n1\na\t0\tx\t1\tNone\nx" },
        { "raw size not numeric", "STARPAK3\n1\na\t0\t1\tx\tNone\nx" },
        { "hash too short", "STARPAK3\n1\na\t0\t1\t1\tNone\t" + shortHex + "\nx" },
        { "hash not hex", "STARPAK3\n1\na\t0\t1\t1\tNone\t" + badHex + "\nx" },
        { "v1 too few fields", "STARPAK1\n1\na\t0\nx" },
        { "v2 too few fields", "STARPAK2\n1\na\t0\t1\t1\nx" },
        { "v2 method not numeric", "STARPAK2\n1\na\t0\t1\t1\tq\nx" },
        { "only tabs", "STARPAK3\n1\n\t\t\t\t\nx" },
        { "crc too short", "STARPAK3\n1\na\t0\t1\t1\tNone\t1234567\nx" },
        { "crc too long", "STARPAK3\n1\na\t0\t1\t1\tNone\t123456789\nx" },
        { "crc not hex", "STARPAK3\n1\na\t0\t1\t1\tNone\t1234567z\nx" },
        { "crc with prefix", "STARPAK3\n1\na\t0\t1\t1\tNone\t0x123456\nx" },
        { "crc negative", "STARPAK3\n1\na\t0\t1\t1\tNone\t-1234567\nx" },
        { "hash length between crc and sha", "STARPAK3\n1\na\t0\t1\t1\tNone\t" + std::string(32, 'a') + "\nx" },
        { "hash too long", "STARPAK3\n1\na\t0\t1\t1\tNone\t" + std::string(65, 'a') + "\nx" },
    };
    for (const auto& c : cases) {
        StarPak::Reader reader;
        CHECK_MSG(!LoadText(reader, c.second), c.first);
        CHECK_MSG(!reader.HasEntry("a"), "no partial state after failed load: " + c.first);
        std::vector<uint8_t> out;
        std::string error;
        CHECK_MSG(!reader.ReadEntryVerified("a", out, error), "no readable entries after failed load: " + c.first);
    }
}

TEST(starpak_failed_load_discards_previous_contents) {
    StarPak::Reader reader;
    REQUIRE(LoadText(reader, "STARPAK3\n1\nold\t0\t1\t1\tNone\nx"));
    CHECK(reader.HasEntry("old"));
    CHECK(!LoadText(reader, "garbage"));
    CHECK(!reader.HasEntry("old"));
    REQUIRE(LoadText(reader, "STARPAK3\n1\nnew\t0\t1\t1\tNone\ny"));
    CHECK(reader.HasEntry("new"));
    CHECK(!reader.HasEntry("old"));
}

TEST(starpak_executable_embedding_round_trip) {
    TempDir dir;
    test::Rng rng(81);
    std::vector<uint8_t> exe = rng.Bytes(5000);
    REQUIRE(test::WriteFile(dir.Path("game.exe"), exe));

    std::vector<Sample> samples = { { "a.txt", ToBytes("embedded pak") }, { "b.bin", Pattern(400, 1) } };
    StarPak::Writer writer;
    for (const Sample& s : samples) writer.AddFile(s.path, s.bytes);
    REQUIRE(writer.WriteToFile(dir.Str("one.pak")));
    REQUIRE(StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("one.pak")));

    std::vector<uint8_t> result = test::ReadFile(dir.Path("game.exe"));
    CHECK(result.size() == exe.size() + test::ReadFile(dir.Path("one.pak")).size() + 24);
    CHECK_BYTES(std::vector<uint8_t>(result.begin(), result.begin() + 5000), exe);

    StarPak::Reader reader;
    REQUIRE(reader.LoadFromExecutable(dir.Str("game.exe")));
    for (const Sample& s : samples) CHECK_BYTES(reader.ReadEntry(s.path), s.bytes);
}

TEST(starpak_second_append_replaces_the_first_pak) {
    TempDir dir;
    test::Rng rng(82);
    std::vector<uint8_t> exe = rng.Bytes(3000);
    REQUIRE(test::WriteFile(dir.Path("game.exe"), exe));

    StarPak::Writer one;
    one.AddFile("first.txt", ToBytes("first"));
    REQUIRE(one.WriteToFile(dir.Str("one.pak")));
    StarPak::Writer two;
    two.AddFile("second.txt", ToBytes("second"));
    two.AddFile("padding.bin", Pattern(500, 6));
    REQUIRE(two.WriteToFile(dir.Str("two.pak")));

    REQUIRE(StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("one.pak")));
    REQUIRE(StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("two.pak")));

    std::vector<uint8_t> result = test::ReadFile(dir.Path("game.exe"));
    CHECK(result.size() == exe.size() + test::ReadFile(dir.Path("two.pak")).size() + 24);
    CHECK_BYTES(std::vector<uint8_t>(result.begin(), result.begin() + 3000), exe);

    StarPak::Reader reader;
    REQUIRE(reader.LoadFromExecutable(dir.Str("game.exe")));
    CHECK(reader.HasEntry("second.txt"));
    CHECK(!reader.HasEntry("first.txt"));

    REQUIRE(StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("one.pak")));
    StarPak::Reader again;
    REQUIRE(again.LoadFromExecutable(dir.Str("game.exe")));
    CHECK(again.HasEntry("first.txt"));
    CHECK(!again.HasEntry("second.txt"));
    CHECK(test::ReadFile(dir.Path("game.exe")).size() == exe.size() + test::ReadFile(dir.Path("one.pak")).size() + 24);
}

TEST(starpak_append_ignores_an_inconsistent_existing_footer) {
    TempDir dir;
    test::Rng rng(83);
    std::vector<uint8_t> exe = Concat({ rng.Bytes(1000), MakeFooter(UINT64_MAX, 12345) });
    REQUIRE(test::WriteFile(dir.Path("game.exe"), exe));

    StarPak::Writer writer;
    writer.AddFile("a.txt", ToBytes("hello"));
    REQUIRE(writer.WriteToFile(dir.Str("a.pak")));
    REQUIRE(StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("a.pak")));

    std::vector<uint8_t> result = test::ReadFile(dir.Path("game.exe"));
    CHECK(result.size() == exe.size() + test::ReadFile(dir.Path("a.pak")).size() + 24);
    CHECK_BYTES(std::vector<uint8_t>(result.begin(), result.begin() + (std::ptrdiff_t)exe.size()), exe);

    StarPak::Reader reader;
    REQUIRE(reader.LoadFromExecutable(dir.Str("game.exe")));
    CHECK_BYTES(reader.ReadEntry("a.txt"), ToBytes("hello"));
}

TEST(starpak_append_failure_cases) {
    TempDir dir;
    REQUIRE(test::WriteFile(dir.Path("game.exe"), Pattern(100, 1)));
    StarPak::Writer writer;
    writer.AddFile("a", { 1 });
    REQUIRE(writer.WriteToFile(dir.Str("a.pak")));

    CHECK(!StarPak::AppendPakToExecutable(dir.Str("game.exe"), dir.Str("missing.pak")));
    CHECK(!StarPak::AppendPakToExecutable(dir.Str("missing.exe"), dir.Str("a.pak")));
    CHECK(test::ReadFile(dir.Path("game.exe")).size() == 100);
}

TEST(starpak_rejects_invalid_executable_footers) {
    TempDir dir;
    StarPak::Writer writer;
    writer.AddFile("a", { 1, 2, 3 });
    REQUIRE(writer.WriteToFile(dir.Str("a.pak")));
    std::vector<uint8_t> pak = test::ReadFile(dir.Path("a.pak"));
    std::vector<uint8_t> prefix = Pattern(200, 5);
    uint64_t pakSize = pak.size();

    struct Case { std::string name; std::vector<uint8_t> bytes; bool shouldLoad; };
    std::vector<Case> cases = {
        { "valid", Concat({ prefix, pak, MakeFooter(200, pakSize) }), true },
        { "valid with offset zero", Concat({ pak, MakeFooter(0, pakSize) }), true },
        { "no footer", Concat({ prefix, pak }), false },
        { "shorter than footer", std::vector<uint8_t>(10, 1), false },
        { "empty file", {}, false },
        { "footer only", MakeFooter(0, 0), false },
        { "wrong footer magic", Concat({ prefix, pak, MakeFooter(200, pakSize, "STARPAKX") }), false },
        { "size zero", Concat({ prefix, pak, MakeFooter(200, 0) }), false },
        { "size too small", Concat({ prefix, pak, MakeFooter(200, pakSize - 1) }), false },
        { "size too large", Concat({ prefix, pak, MakeFooter(200, pakSize + 1) }), false },
        { "offset too large", Concat({ prefix, pak, MakeFooter(201, pakSize) }), false },
        { "offset too small", Concat({ prefix, pak, MakeFooter(199, pakSize) }), false },
        { "offset beyond file", Concat({ prefix, pak, MakeFooter(UINT64_MAX, pakSize) }), false },
        { "size beyond file", Concat({ prefix, pak, MakeFooter(200, UINT64_MAX) }), false },
        { "offset plus size overflows", Concat({ prefix, pak, MakeFooter(UINT64_MAX - 3, 10) }), false },
        { "pak region is not a pak", Concat({ prefix, Pattern(40, 9), MakeFooter(200, 40) }), false },
    };
    for (const Case& c : cases) {
        REQUIRE(test::WriteFile(dir.Path("probe.bin"), c.bytes));
        StarPak::Reader reader;
        CHECK_MSG(reader.LoadFromExecutable(dir.Str("probe.bin")) == c.shouldLoad, c.name);
        if (!c.shouldLoad) CHECK_MSG(!reader.HasEntry("a"), c.name);
    }
    StarPak::Reader missing;
    CHECK(!missing.LoadFromExecutable(dir.Str("not_there.bin")));
}

TEST(starpak_survives_mutated_paks) {
    TempDir dir;
    std::vector<Sample> samples = {
        { "Content/a.bin", Pattern(300, 1) },
        { "Content/b.bin", Pattern(70, 2) },
        { "Content/empty.bin", {} },
        { "__boot__", ToBytes("boot") },
    };
    std::vector<uint8_t> original = BuildPakBytes(samples, dir);
    std::vector<std::string> paths;
    for (const Sample& s : samples) paths.push_back(s.path);
    paths.push_back("a");
    paths.push_back("");

    test::Rng rng(2468);
    for (int i = 0; i < 6000; i++) {
        std::vector<uint8_t> data = original;
        int edits = 1 + (int)rng.Below(4);
        for (int e = 0; e < edits; e++) {
            switch (rng.Below(6)) {
                case 0: data[rng.Below((uint32_t)data.size())] ^= (uint8_t)(1u << rng.Below(8)); break;
                case 1: data[rng.Below((uint32_t)data.size())] = (uint8_t)rng.Next(); break;
                case 2: data.erase(data.begin() + (std::ptrdiff_t)rng.Below((uint32_t)data.size())); break;
                case 3: data.insert(data.begin() + (std::ptrdiff_t)rng.Below((uint32_t)data.size() + 1), (uint8_t)rng.Next()); break;
                case 4: data.resize(rng.Below((uint32_t)data.size() + 1)); break;
                default: {
                    size_t header = std::min<size_t>(data.size(), 200);
                    if (header) data[rng.Below((uint32_t)header)] = "0123456789\t\n-xz"[rng.Below(15)];
                    break;
                }
            }
            if (data.empty()) data.push_back(0);
        }
        StarPak::Reader reader;
        if (reader.LoadBuffer(data)) {
            CheckReaderBounded(reader, paths);
        } else {
            for (const std::string& path : paths) CHECK(!reader.HasEntry(path));
        }
    }
}

TEST(starpak_survives_mutated_executables) {
    TempDir dir;
    StarPak::Writer writer;
    writer.AddFile("a", Pattern(60, 1));
    writer.AddFile("b", Pattern(10, 2));
    REQUIRE(writer.WriteToFile(dir.Str("a.pak")));
    std::vector<uint8_t> pak = test::ReadFile(dir.Path("a.pak"));
    std::vector<uint8_t> original = Concat({ Pattern(500, 3), pak, MakeFooter(500, pak.size()) });

    test::Rng rng(1357);
    std::vector<std::string> paths = { "a", "b", "c" };
    for (int i = 0; i < 1500; i++) {
        std::vector<uint8_t> data = original;
        int edits = 1 + (int)rng.Below(3);
        for (int e = 0; e < edits; e++) {
            size_t tailStart = data.size() > 40 ? data.size() - 40 : 0;
            size_t pos = tailStart + rng.Below((uint32_t)(data.size() - tailStart));
            if (rng.Below(3) == 0) data[pos] = (uint8_t)rng.Next();
            else data[pos] ^= (uint8_t)(1u << rng.Below(8));
        }
        if (rng.Below(5) == 0) data.resize(rng.Below((uint32_t)data.size() + 1));
        REQUIRE(test::WriteFile(dir.Path("mut.bin"), data));
        StarPak::Reader reader;
        if (reader.LoadFromExecutable(dir.Str("mut.bin"))) CheckReaderBounded(reader, paths);
    }
}

std::vector<uint8_t> BuildKeyedPakBytes(const std::vector<Sample>& samples, const TempDir& dir, const std::string& key) {
    StarPak::Writer writer;
    writer.SetIndexKey(key);
    for (const Sample& s : samples) writer.AddFile(s.path, s.bytes);
    std::string file = dir.Str("keyed.pak");
    writer.WriteToFile(file);
    return test::ReadFile(file);
}

bool Contains(const std::vector<uint8_t>& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

TEST(starpak_keyed_index_round_trips_with_the_right_key) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();
    std::vector<uint8_t> bytes = BuildKeyedPakBytes(samples, dir, "correct horse");
    CHECK(std::memcmp(bytes.data(), "STARPAK4", 8) == 0);

    StarPak::Reader reader;
    REQUIRE(reader.LoadBuffer(bytes, "correct horse"));
    for (const Sample& s : samples) {
        std::vector<uint8_t> out;
        std::string error;
        CHECK_MSG(reader.ReadEntryVerified(s.path, out, error), s.path + ": " + error);
        CHECK_BYTES(out, s.bytes);
    }

    StarPak::Reader fromFile;
    REQUIRE(fromFile.LoadFile(dir.Path("keyed.pak").string(), "correct horse"));
    CHECK(fromFile.HasEntry("__boot__"));
}

TEST(starpak_keyed_index_hides_names_and_rejects_wrong_keys) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();
    std::vector<uint8_t> bytes = BuildKeyedPakBytes(samples, dir, "correct horse");

    for (const Sample& s : samples) {
        if (s.path.size() < 6) continue;
        CHECK_MSG(!Contains(bytes, s.path), "path visible: " + s.path);
    }
    CHECK(!Contains(bytes, "Content/"));
    CHECK(!Contains(bytes, "Deflate"));

    for (const char* wrong : { "", "correct horse ", "Correct horse", "wrong", "correct  horse" }) {
        StarPak::Reader reader;
        CHECK_MSG(!reader.LoadBuffer(bytes, wrong), wrong);
        CHECK(!reader.HasEntry("__boot__"));
    }
}

TEST(starpak_keyed_index_keeps_plain_paks_readable_without_a_key) {
    TempDir dir;
    std::vector<Sample> samples = StandardSamples();
    std::vector<uint8_t> plain = BuildPakBytes(samples, dir);
    CHECK(std::memcmp(plain.data(), "STARPAK3", 8) == 0);
    CHECK(Contains(plain, "Content/Textures/wall.png"));

    StarPak::Reader noKey;
    CHECK(noKey.LoadBuffer(plain));
    StarPak::Reader ignoredKey;
    CHECK(ignoredKey.LoadBuffer(plain, "anything"));
    CHECK(ignoredKey.HasEntry("__boot__"));
}

TEST(starpak_keyed_index_detects_any_index_damage) {
    TempDir dir;
    std::vector<Sample> samples = {
        { "a.txt", ToBytes("hello") },
        { "b.bin", Pattern(40, 2) },
    };
    std::vector<uint8_t> bytes = BuildKeyedPakBytes(samples, dir, "key");

    size_t headerEnd = 9;
    while (bytes[headerEnd] != '\n') headerEnd++;
    headerEnd++;
    const size_t blobStart = bytes.size() - 45;
    REQUIRE(blobStart > headerEnd);

    for (size_t i = 0; i < blobStart; i++) {
        for (int bit = 0; bit < 8; bit += 3) {
            std::vector<uint8_t> damaged = bytes;
            damaged[i] ^= (uint8_t)(1 << bit);
            StarPak::Reader reader;
            if (i < 9) {
                CHECK_MSG(!reader.LoadBuffer(damaged, "key"), "magic byte " + std::to_string(i));
                continue;
            }
            const bool loaded = reader.LoadBuffer(damaged, "key");
            if (i >= headerEnd) CHECK_MSG(!loaded, "index byte " + std::to_string(i) + " bit " + std::to_string(bit));
        }
    }
}

TEST(starpak_keyed_index_rejects_malformed_headers) {
    const std::string body(64, 'x');
    const std::vector<std::pair<const char*, std::string>> cases = {
        { "magic only", "STARPAK4" },
        { "no newline after header", "STARPAK4\n10\t00000000" },
        { "missing crc", "STARPAK4\n10\n" + body },
        { "extra field", "STARPAK4\n10\t00000000\t1\n" + body },
        { "empty size", "STARPAK4\n\t00000000\n" + body },
        { "non digit size", "STARPAK4\n1x\t00000000\n" + body },
        { "negative size", "STARPAK4\n-5\t00000000\n" + body },
        { "zero size", "STARPAK4\n0\t00000000\n" + body },
        { "size beyond buffer", "STARPAK4\n99999\t00000000\n" + body },
        { "size overflow", "STARPAK4\n99999999999999999999999\t00000000\n" + body },
        { "crc too short", "STARPAK4\n10\t1234567\n" + body },
        { "crc not hex", "STARPAK4\n10\t1234567z\n" + body },
        { "wrong crc", "STARPAK4\n10\t00000000\n" + body },
    };
    for (const auto& c : cases) {
        StarPak::Reader reader;
        std::vector<uint8_t> buffer(c.second.begin(), c.second.end());
        CHECK_MSG(!reader.LoadBuffer(buffer, "key"), c.first);
        CHECK_MSG(!reader.HasEntry("a"), c.first);
    }
}

TEST(starpak_keyed_index_survives_random_damage_without_crashing) {
    TempDir dir;
    std::vector<uint8_t> bytes = BuildKeyedPakBytes(StandardSamples(), dir, "fuzz key");
    test::Rng rng(31337);
    for (int round = 0; round < 400; round++) {
        std::vector<uint8_t> damaged = bytes;
        const int edits = 1 + (int)rng.Below(6);
        for (int e = 0; e < edits; e++) damaged[rng.Below((uint32_t)damaged.size())] = (uint8_t)rng.Next();
        if (rng.Below(4) == 0) damaged.resize(rng.Below((uint32_t)damaged.size()));

        StarPak::Reader reader;
        if (!reader.LoadBuffer(damaged, "fuzz key")) continue;
        for (const Sample& s : StandardSamples()) {
            std::vector<uint8_t> out;
            std::string error;
            reader.ReadEntryVerified(s.path, out, error);
        }
    }
}

TEST_MAIN()
