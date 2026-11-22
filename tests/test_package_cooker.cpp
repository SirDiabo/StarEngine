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

std::string UnpackedEntry(const StarPak::Reader& pak, const std::string& key, const std::string& path) {
    std::string error;
    std::vector<uint8_t> raw = ReadEntry(pak, key, path, error);
    std::string text;
    if (raw.empty() || !CookedText::LooksCooked(raw.data(), raw.size()) || !CookedText::Unpack(raw.data(), raw.size(), text)) return std::string();
    return text;
}

bool HasLine(const std::string& text, const std::string& line) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        if (text.compare(start, end - start, line) == 0 && end - start == line.size()) return true;
        start = end + 1;
    }
    return false;
}

TEST(cooker_packs_animation_assets_and_pulls_their_outside_source_into_the_pak) {
    TempDir dir;
    const std::string external = dir.Str("External/wave.glb");
    test::WriteFile(dir.Path("External/wave.glb"), Bytes("EXTERNAL-GLB"));
    test::WriteText(dir.Path("Content/Animations/Wave.staranim"),
        "[StarAsset]\nType=Animation\nSource=" + external + "\nVendoredSource=\nAnimName=Wave\nAnimSourceIndex=0\n");

    const std::string key = "anim key";
    std::vector<std::string> seen;
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(&seen), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    const std::string text = UnpackedEntry(pak, key, "Animations/Wave.staranim");
    REQUIRE(!text.empty());
    CHECK(HasLine(text, "Source=_Imported/wave.glb"));
    CHECK(HasLine(text, "AnimName=Wave"));
    CHECK(!Contains(Bytes(text), external));

    std::string error;
    CHECK_BYTES(ReadEntry(pak, key, "_Imported/wave.glb", error), Bytes("COOKED-MODEL:wave.glb"));
    CHECK(seen.size() == 1);
}

TEST(cooker_points_animation_assets_at_their_vendored_copy_when_there_is_one) {
    TempDir dir;
    const std::string external = dir.Str("External/wave.glb");
    test::WriteFile(dir.Path("External/wave.glb"), Bytes("EXTERNAL-GLB"));
    test::WriteFile(dir.Path("Content/_Source/Wave_Wave/wave.glb"), Bytes("VENDORED-GLB"));
    test::WriteText(dir.Path("Content/Animations/Wave.staranim"),
        "[StarAsset]\nType=Animation\nSource=" + external + "\nVendoredSource=_Source/Wave_Wave/wave.glb\nAnimName=Wave\n");

    const std::string key = "anim key";
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(nullptr), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    const std::string text = UnpackedEntry(pak, key, "Animations/Wave.staranim");
    REQUIRE(!text.empty());
    CHECK(HasLine(text, "Source=_Source/Wave_Wave/wave.glb"));
    CHECK(HasLine(text, "VendoredSource=_Source/Wave_Wave/wave.glb"));
    CHECK(!pak.HasEntry("_Imported/wave.glb"));
    CHECK(!Contains(Bytes(text), external));
}

TEST(cooker_packs_sound_assets_and_pulls_their_outside_source_into_the_pak) {
    TempDir dir;
    const std::string external = dir.Str("External/hit.wav");
    test::WriteFile(dir.Path("External/hit.wav"), Bytes("RIFF-EXTERNAL-WAV"));
    test::WriteText(dir.Path("Content/Sounds/Hit.starsound"),
        "[StarAsset]\nType=Sound\nSource=" + external + "\nVendoredSource=\nSoundVolume=0.5\n");

    const std::string key = "sound key";
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(nullptr), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    const std::string text = UnpackedEntry(pak, key, "Sounds/Hit.starsound");
    REQUIRE(!text.empty());
    CHECK(HasLine(text, "Source=_Imported/hit.wav"));
    CHECK(HasLine(text, "SoundVolume=0.5"));

    std::string error;
    CHECK_BYTES(ReadEntry(pak, key, "_Imported/hit.wav", error), Bytes("RIFF-EXTERNAL-WAV"));
}

TEST(cooker_shares_one_cooked_copy_between_a_mesh_asset_and_its_animation_assets) {
    TempDir dir;
    const std::string external = dir.Str("External/hero.glb");
    test::WriteFile(dir.Path("External/hero.glb"), Bytes("EXTERNAL-GLB"));
    test::WriteText(dir.Path("Content/Meshes/Hero.starasset"), "[StarAsset]\nType=StaticMesh\nSource=" + external + "\nVendoredSource=\n");
    test::WriteText(dir.Path("Content/Animations/Hero_Run.staranim"), "[StarAsset]\nType=Animation\nSource=" + external + "\nVendoredSource=\nAnimName=Run\n");
    test::WriteText(dir.Path("Content/Animations/Hero_Idle.staranim"), "[StarAsset]\nType=Animation\nSource=" + external + "\nVendoredSource=\nAnimName=Idle\n");

    const std::string key = "share key";
    std::vector<std::string> seen;
    CookResult result;
    REQUIRE(Cook(dir, key, PackCompressions::None, FakeMeshCooker(&seen), result));

    StarPak::Reader pak;
    REQUIRE(pak.LoadFile(dir.Str("out/game.starpak"), key));
    CHECK(HasLine(UnpackedEntry(pak, key, "Meshes/Hero.starasset"), "Source=_Imported/hero.glb"));
    CHECK(HasLine(UnpackedEntry(pak, key, "Animations/Hero_Run.staranim"), "Source=_Imported/hero.glb"));
    CHECK(HasLine(UnpackedEntry(pak, key, "Animations/Hero_Idle.staranim"), "Source=_Imported/hero.glb"));
    CHECK(seen.size() == 1);
}

TEST_MAIN()
