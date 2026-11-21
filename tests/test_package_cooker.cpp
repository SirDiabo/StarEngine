#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/CookedEntry.h"
#include "Core/CookedText.h"
#include "Core/PackageCipher.h"
#include "Core/PackageCooker.h"
#include "Core/StarPak.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using test::TempDir;

const char* kActorText = "[Actor]\nName=Crate\nHealth=100\nMesh=Meshes/crate.glb\n";
const char* kMapText = "Level=Main\nActors=2\n";

std::vector<uint8_t> Bytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

bool Contains(const std::vector<uint8_t>& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

void BuildProject(const TempDir& dir) {
    test::WriteText(dir.Path("Content/Actors/Crate.staractor"), kActorText);
    test::WriteText(dir.Path("Content/Levels/Main.starmap"), kMapText);
    test::WriteFile(dir.Path("Content/Meshes/crate.glb"), Bytes("SOURCE-GLB-DATA-DO-NOT-SHIP"));
    test::WriteFile(dir.Path("Content/Meshes/ship.obj"), Bytes("v 0 0 0\nSOURCE-OBJ-DATA\n"));
    test::WriteFile(dir.Path("Content/Meshes/ship.mtl"), Bytes("newmtl Hull\nSOURCE-MTL-DATA\n"));
    test::WriteFile(dir.Path("Content/Textures/wall.png"), Bytes("PNG-BYTES-1234567890"));
}

PackageCooker::MeshCooker FakeMeshCooker(std::vector<std::string>* seen, bool fail = false) {
    return [seen, fail](const std::string& diskPath, std::vector<uint8_t>& out, std::string& error) {
        if (seen) seen->push_back(diskPath);
        if (fail) {
            error = "synthetic failure";
            return false;
        }
        out = Bytes("COOKED-MODEL:");
        const std::string name = diskPath.substr(diskPath.find_last_of("/\\") + 1);
        out.insert(out.end(), name.begin(), name.end());
        return true;
    };
}

std::vector<PackageCooker::ExtraPakFile> Extras() {
    return { { StarPak::kIconPath, Bytes("ICON-PNG-BYTES-0123456789") }, { StarPak::kInputMappingsPath, Bytes("jump=Space\n") } };
}

bool Cook(const TempDir& dir, const std::string& key, const std::string& compression, const PackageCooker::MeshCooker& cooker, CookResult& result) {
    result = PackageCooker::CookToPak(dir.Str("Content"), dir.Str("out/game.starpak"), CookCiphers::ChaCha12, key,
                                       "name=Crate Game\nrenderScale=1\n", Extras(), compression, cooker);
    return result.success;
}

std::vector<uint8_t> ReadEntry(const StarPak::Reader& pak, const std::string& key, const std::string& path, std::string& error) {
    std::vector<uint8_t> out;
    if (!CookedEntry::Read(pak, CookCiphers::ChaCha12, key, path, out, error)) out.clear();
    return out;
}

}

TEST(cooker_writes_an_encrypted_pak_with_cooked_meshes_and_packed_text) {
    TempDir dir;
    BuildProject(dir);
    const std::string key = "cooker key";
    std::vector<std::string> seen;
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(&seen), result));
    CHECK(result.error.empty());

    const std::vector<uint8_t> file = test::ReadFile(dir.Path("out/game.starpak"));
    REQUIRE(!file.empty());
    CHECK(std::memcmp(file.data(), "STARPAK4", 8) == 0);

    StarPak::Reader noKey;
    CHECK(!noKey.LoadBuffer(file));
    StarPak::Reader wrongKey;
    CHECK(!wrongKey.LoadBuffer(file, "other"));

    StarPak::Reader pak;
    REQUIRE(pak.LoadBuffer(file, key));
    std::string error;

    std::vector<uint8_t> actor = ReadEntry(pak, key, "Actors/Crate.staractor", error);
    REQUIRE(!actor.empty());
    CHECK(CookedText::LooksCooked(actor.data(), actor.size()));
    std::string actorText;
    REQUIRE(CookedText::Unpack(actor.data(), actor.size(), actorText));
    CHECK_EQ_STR(actorText, kActorText);

    std::vector<uint8_t> map = ReadEntry(pak, key, "Levels/Main.starmap", error);
    std::string mapText;
    REQUIRE(CookedText::Unpack(map.data(), map.size(), mapText));
    CHECK_EQ_STR(mapText, kMapText);

    CHECK_BYTES(ReadEntry(pak, key, "Meshes/crate.glb", error), Bytes("COOKED-MODEL:crate.glb"));
    CHECK_BYTES(ReadEntry(pak, key, "Meshes/ship.obj", error), Bytes("COOKED-MODEL:ship.obj"));
    CHECK(!pak.HasEntry("Meshes/ship.mtl"));
    CHECK_BYTES(ReadEntry(pak, key, "Textures/wall.png", error), Bytes("PNG-BYTES-1234567890"));

    CHECK(seen.size() == 2);
    CHECK(result.filesCooked == 5);

    for (const char* secret : { "SOURCE-GLB-DATA", "SOURCE-OBJ-DATA", "SOURCE-MTL-DATA", "PNG-BYTES", "ICON-PNG", "jump=Space",
                                "Crate Game", "renderScale", "Health", "Actors/Crate", "wall.png" }) {
        CHECK_MSG(!Contains(file, secret), secret);
    }
}

TEST(cooker_encrypts_the_boot_config_and_extra_files) {
    TempDir dir;
    BuildProject(dir);
    const std::string key = "extras key";
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(nullptr), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    std::string error;
    CHECK_BYTES(ReadEntry(pak, key, StarPak::kBootConfigPath, error), Bytes("name=Crate Game\nrenderScale=1\n"));
    CHECK_BYTES(ReadEntry(pak, key, StarPak::kIconPath, error), Bytes("ICON-PNG-BYTES-0123456789"));
    CHECK_BYTES(ReadEntry(pak, key, StarPak::kInputMappingsPath, error), Bytes("jump=Space\n"));

    CHECK(!test::BytesEqual(pak.ReadEntry(StarPak::kBootConfigPath), Bytes("name=Crate Game\nrenderScale=1\n")));
    CHECK(!test::BytesEqual(pak.ReadEntry(StarPak::kIconPath), Bytes("ICON-PNG-BYTES-0123456789")));
}

TEST(cooker_compresses_entries_when_asked_and_they_still_decode) {
    TempDir dir;
    BuildProject(dir);
    std::string big;
    for (int i = 0; i < 400; i++) big += "Actor=Crate\nClass=Prop\n";
    test::WriteText(dir.Path("Content/Levels/Big.starmap"), big);

    const std::string key = "compress key";
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::Deflate, FakeMeshCooker(nullptr), result));
    CHECK(result.filesCompressed > 0);
    CHECK(result.storedBytes < result.rawBytes);

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    StarPak::Entry info;
    REQUIRE(pak.GetEntryInfo("Levels/Big.starmap", info));
    CHECK(info.compression == PackCompressions::Deflate);

    std::string error;
    std::vector<uint8_t> packed = ReadEntry(pak, key, "Levels/Big.starmap", error);
    REQUIRE(!packed.empty());
    std::string text;
    REQUIRE(CookedText::Unpack(packed.data(), packed.size(), text));
    CHECK_EQ_STR(text, big);
}

TEST(cooker_stops_and_names_the_file_when_a_mesh_cannot_be_cooked) {
    TempDir dir;
    BuildProject(dir);
    CookResult result;
    CHECK(!Cook(dir, "k", PackCompressions::None, FakeMeshCooker(nullptr, true), result));
    CHECK(!result.success);
    CHECK(result.error.find("Could not cook mesh") != std::string::npos);
    CHECK(result.error.find("synthetic failure") != std::string::npos);
    CHECK(result.error.find(".glb") != std::string::npos || result.error.find(".obj") != std::string::npos);
}

TEST(cooker_gives_every_text_file_its_own_scramble) {
    TempDir dir;
    test::WriteText(dir.Path("Content/A.starmap"), kMapText);
    test::WriteText(dir.Path("Content/B.starmap"), kMapText);
    const std::string key = "k";
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(nullptr), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    std::string error;
    std::vector<uint8_t> a = ReadEntry(pak, key, "A.starmap", error);
    std::vector<uint8_t> b = ReadEntry(pak, key, "B.starmap", error);
    REQUIRE(!a.empty() && !b.empty());
    CHECK(!test::BytesEqual(a, b));
}

TEST(cooker_rejects_a_missing_content_folder_and_unknown_cipher) {
    TempDir dir;
    CookResult result = PackageCooker::CookToPak(dir.Str("NoContent"), dir.Str("out/a.starpak"), CookCiphers::ChaCha12, "k",
                                                  "boot", {}, PackCompressions::None, FakeMeshCooker(nullptr));
    CHECK(!result.success);
    CHECK(!result.error.empty());

    BuildProject(dir);
    result = PackageCooker::CookToPak(dir.Str("Content"), dir.Str("out/b.starpak"), "NoSuchCipher", "k",
                                       "boot", {}, PackCompressions::None, FakeMeshCooker(nullptr));
    CHECK(!result.success);
    CHECK(result.error.find("NoSuchCipher") != std::string::npos);
}

TEST(loose_cook_is_the_readable_debug_format) {
    TempDir dir;
    BuildProject(dir);
    CookResult result = PackageCooker::CookLoose(dir.Str("Content"), dir.Str("loose"));
    REQUIRE(result.success);

    CHECK_EQ_STR(test::ReadText(dir.Path("loose/Actors/Crate.staractor")), kActorText);
    CHECK_BYTES(test::ReadFile(dir.Path("loose/Meshes/crate.glb")), Bytes("SOURCE-GLB-DATA-DO-NOT-SHIP"));
    CHECK_BYTES(test::ReadFile(dir.Path("loose/Meshes/ship.obj")), Bytes("v 0 0 0\nSOURCE-OBJ-DATA\n"));
    CHECK(std::filesystem::exists(dir.Path("loose/Meshes/ship.mtl")));
    CHECK_BYTES(test::ReadFile(dir.Path("loose/Textures/wall.png")), Bytes("PNG-BYTES-1234567890"));
}

TEST_MAIN()
