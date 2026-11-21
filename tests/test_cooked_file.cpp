#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/ContentPathResolver.h"
#include "Core/CookedFile.h"
#include "Core/CookedText.h"
#include "Core/PackageCipher.h"
#include "Core/StarPak.h"
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace {

using test::TempDir;

std::vector<uint8_t> Bytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string ReadAll(std::istream& in) {
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool AddCooked(StarPak::Writer& writer, const std::string& path, std::vector<uint8_t> bytes, const std::string& key) {
    CreateCookCipher(CookCiphers::ChaCha12)->Encrypt(bytes, key, path);
    return writer.AddFile(path, bytes);
}

const char* kText = "Name=Hero\nSpeed=4\n";

}

TEST(cooked_file_loose_mode_reads_from_the_content_folder) {
    TempDir dir;
    test::WriteText(dir.Path("Content/Actors/Hero.staractor"), kText);
    test::WriteFile(dir.Path("Content/Sounds/a.wav"), Bytes("RIFFDATA"));
    ContentPathResolver::SetContentRoot(dir.Str("Content"));

    CHECK(!CookedFile::IsUsingPak());
    CHECK(CookedFile::Exists("Actors/Hero.staractor"));
    CHECK(!CookedFile::Exists("Actors/Missing.staractor"));
    CHECK(!CookedFile::Exists("Actors"));

    const std::string disk = CookedFile::DiskPath("Actors/Hero.staractor");
    CHECK(!disk.empty());
    CHECK(std::filesystem::exists(disk));
    CHECK_EQ_STR(CookedFile::Key("Actors/Hero.staractor"), disk);

    std::string text;
    REQUIRE(CookedFile::ReadText("Actors/Hero.staractor", text));
    CHECK_EQ_STR(text, kText);

    std::vector<uint8_t> bytes;
    REQUIRE(CookedFile::ReadBytes("Sounds/a.wav", bytes));
    CHECK_BYTES(bytes, Bytes("RIFFDATA"));

    std::shared_ptr<const std::vector<uint8_t>> shared = CookedFile::ReadShared("Sounds/a.wav");
    REQUIRE(shared != nullptr);
    CHECK_BYTES(*shared, Bytes("RIFFDATA"));

    std::unique_ptr<std::istream> stream = CookedFile::OpenStream("Actors/Hero.staractor");
    REQUIRE(stream != nullptr);
    CHECK_EQ_STR(ReadAll(*stream), kText);

    CHECK(!CookedFile::ReadBytes("Nope.bin", bytes));
    CHECK(bytes.empty());
    CHECK(!CookedFile::ReadText("Nope.bin", text));
    CHECK(text.empty());
    CHECK(CookedFile::ReadShared("Nope.bin") == nullptr);
    CHECK(CookedFile::OpenStream("Nope.bin") == nullptr);
}

TEST(cooked_file_loose_mode_does_not_unpack_plain_files_that_look_odd) {
    TempDir dir;
    test::WriteText(dir.Path("Content/odd.txt"), "STXT1 but really just text");
    ContentPathResolver::SetContentRoot(dir.Str("Content"));
    std::string text;
    REQUIRE(CookedFile::ReadText("odd.txt", text));
    CHECK_EQ_STR(text, "STXT1 but really just text");
}

TEST(cooked_file_pak_mode_reads_everything_from_memory) {
    TempDir dir;
    const std::string key = "facade key";

    std::vector<uint8_t> packedText;
    REQUIRE(CookedText::Pack(kText, 99, packedText));

    StarPak::Writer writer;
    writer.SetIndexKey(key);
    REQUIRE(AddCooked(writer, "Actors/Hero.staractor", packedText, key));
    REQUIRE(AddCooked(writer, "Textures/wall.png", Bytes("PNGDATA"), key));
    REQUIRE(AddCooked(writer, "Sounds/a.wav", Bytes("RIFFDATA"), key));
    REQUIRE(writer.WriteToFile(dir.Str("game.starpak")));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("game.starpak"), key));
    CookedFile::SetActivePak(std::move(pak), CookCiphers::ChaCha12, key);

    ContentPathResolver::SetContentRoot(dir.Str("DoesNotExist"));
    CHECK(CookedFile::IsUsingPak());
    CHECK(CookedFile::Exists("Actors/Hero.staractor"));
    CHECK(CookedFile::Exists("Actors\\Hero.staractor"));
    CHECK(!CookedFile::Exists("Actors/Missing.staractor"));
    CHECK(CookedFile::DiskPath("Actors/Hero.staractor").empty());
    CHECK_EQ_STR(CookedFile::Key("Actors\\Hero.staractor"), "Actors/Hero.staractor");
    CHECK(CookedFile::ModifiedTime("Actors/Hero.staractor") == 0);

    std::string text;
    REQUIRE(CookedFile::ReadText("Actors/Hero.staractor", text));
    CHECK_EQ_STR(text, kText);

    std::unique_ptr<std::istream> stream = CookedFile::OpenStream("Actors/Hero.staractor");
    REQUIRE(stream != nullptr);
    std::string line;
    std::getline(*stream, line);
    CHECK_EQ_STR(line, "Name=Hero");
    std::getline(*stream, line);
    CHECK_EQ_STR(line, "Speed=4");
    CHECK(!std::getline(*stream, line));

    std::vector<uint8_t> bytes;
    REQUIRE(CookedFile::ReadBytes("Textures/wall.png", bytes));
    CHECK_BYTES(bytes, Bytes("PNGDATA"));
    CHECK(!CookedText::LooksCooked(bytes.data(), bytes.size()));

    REQUIRE(CookedFile::ReadBytes("Actors/Hero.staractor", bytes));
    CHECK_BYTES(bytes, packedText);

    std::shared_ptr<const std::vector<uint8_t>> shared = CookedFile::ReadShared("Sounds/a.wav");
    REQUIRE(shared != nullptr);
    CHECK_BYTES(*shared, Bytes("RIFFDATA"));

    CHECK(!CookedFile::ReadBytes("Nope.bin", bytes));
    CHECK(bytes.empty());
    CHECK(!CookedFile::ReadText("Nope.bin", text));
    CHECK(CookedFile::OpenStream("Nope.bin") == nullptr);
    CHECK(CookedFile::ReadShared("Nope.bin") == nullptr);
}

TEST(cooked_file_pak_mode_never_writes_decrypted_files_to_disk) {
    TempDir dir;
    const std::string key = "no disk";
    StarPak::Writer writer;
    writer.SetIndexKey(key);
    REQUIRE(AddCooked(writer, "Secret/data.bin", Bytes("TOP-SECRET-PAYLOAD"), key));
    REQUIRE(writer.WriteToFile(dir.Str("secret.starpak")));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("secret.starpak"), key));
    CookedFile::SetActivePak(std::move(pak), CookCiphers::ChaCha12, key);

    std::vector<uint8_t> bytes;
    REQUIRE(CookedFile::ReadBytes("Secret/data.bin", bytes));
    CHECK_BYTES(bytes, Bytes("TOP-SECRET-PAYLOAD"));

    int copies = 0;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(std::filesystem::temp_directory_path(), std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        std::error_code sizeEc;
        if (!it->is_regular_file(sizeEc) || sizeEc) continue;
        if (it->path().filename() != "data.bin") continue;
        std::vector<uint8_t> content = test::ReadFile(it->path());
        if (test::BytesEqual(content, Bytes("TOP-SECRET-PAYLOAD"))) copies++;
    }
    CHECK(copies == 0);
}

TEST(cooked_file_pak_mode_reports_damaged_text) {
    TempDir dir;
    const std::string key = "damaged";
    std::vector<uint8_t> packed;
    REQUIRE(CookedText::Pack(kText, 5, packed));
    packed.back() ^= 0x55;
    packed.push_back(0x01);

    StarPak::Writer writer;
    writer.SetIndexKey(key);
    REQUIRE(AddCooked(writer, "bad.staractor", packed, key));
    REQUIRE(writer.WriteToFile(dir.Str("bad.starpak")));
    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("bad.starpak"), key));
    CookedFile::SetActivePak(std::move(pak), CookCiphers::ChaCha12, key);

    std::string text = "stale";
    CHECK(!CookedFile::ReadText("bad.staractor", text));
    CHECK(text.empty());
    CHECK(CookedFile::OpenStream("bad.staractor") == nullptr);
}

TEST_MAIN()
