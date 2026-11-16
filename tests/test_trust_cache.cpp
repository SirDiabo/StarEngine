#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/BuildCacheGuard.h"
#include "Core/TrustStore.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using test::TempDir;

bool MakeSymlink(const fs::path& target, const fs::path& link) {
    std::error_code ec;
    fs::create_directory_symlink(target, link, ec);
    return !ec;
}

fs::path MakeProject(const TempDir& dir, const std::string& name) {
    fs::path root = dir.Path(name);
    std::error_code ec;
    fs::create_directories(root, ec);
    return root;
}

bool IsHex64(const std::string& text) {
    if (text.size() != 64) return false;
    for (char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

const std::string kToken = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const std::string kOtherToken = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

}

TEST(trust_is_off_by_default_and_remembered_per_path) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path projectA = MakeProject(dir, "A");
    fs::path projectB = MakeProject(dir, "B");

    CHECK(!store.IsTrusted(projectA.string()));
    CHECK(store.Trust(projectA.string()));
    CHECK(store.IsTrusted(projectA.string()));
    CHECK(!store.IsTrusted(projectB.string()));

    TrustStore reopened(dir.Str("home"));
    CHECK(reopened.IsTrusted(projectA.string()));
    CHECK(!reopened.IsTrusted(projectB.string()));

    CHECK(store.Trust(projectB.string()));
    CHECK(reopened.IsTrusted(projectB.string()));
}

TEST(trust_can_be_revoked) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path project = MakeProject(dir, "A");
    REQUIRE(store.Trust(project.string()));
    CHECK(store.Revoke(project.string()));
    CHECK(!store.IsTrusted(project.string()));
    CHECK(store.Revoke(project.string()));
    CHECK(store.TrustedKeys().empty());
}

TEST(trust_does_not_duplicate_entries) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path project = MakeProject(dir, "A");
    REQUIRE(store.Trust(project.string()));
    REQUIRE(store.Trust(project.string()));
    REQUIRE(store.Trust((project / ".." / "A").string()));
    CHECK(store.TrustedKeys().size() == 1);
    CHECK(!fs::exists(dir.Path("home") / "trusted_projects.cfg.tmp"));
    std::string content = test::ReadText(dir.Path("home") / "trusted_projects.cfg");
    CHECK(content.rfind("StarEngineTrust1\n", 0) == 0);
}

TEST(trust_matches_equivalent_spellings_of_the_same_folder) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path project = MakeProject(dir, "Game");
    std::error_code ec;
    fs::create_directories(project / "Sub", ec);
    REQUIRE(store.Trust(project.string()));

    CHECK(store.IsTrusted(project.string() + "/"));
    CHECK(store.IsTrusted((project / "Sub" / "..").string()));
    CHECK(store.IsTrusted((project / "." / "").string()));
    CHECK(store.IsTrusted((dir.path / "Game" / ".." / "Game").string()));
    CHECK(!store.IsTrusted((project / "Sub").string()));
    CHECK(!store.IsTrusted(dir.path.string()));
}

TEST(trust_follows_the_real_location_through_symlinks) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path real = MakeProject(dir, "Real");
    fs::path other = MakeProject(dir, "Other");
    if (!MakeSymlink(real, dir.Path("LinkToReal"))) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }
    REQUIRE(store.Trust(real.string()));
    CHECK(store.IsTrusted(dir.Str("LinkToReal")));

    fs::path link2 = dir.Path("LinkToOther");
    REQUIRE(MakeSymlink(other, link2));
    CHECK(!store.IsTrusted(link2.string()));
    REQUIRE(store.Trust(link2.string()));
    CHECK(store.IsTrusted(other.string()));
}

TEST(trust_rejects_unusable_paths) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    CHECK(!store.Trust(""));
    CHECK(!store.IsTrusted(""));
    CHECK(!store.Trust(std::string(5000, 'a')));
    CHECK(!store.Trust(std::string("bad\0name", 8)));
    CHECK(!store.Trust("line\nbreak"));
    CHECK(!store.Revoke(""));
    CHECK(store.TrustedKeys().empty());
}

TEST(trust_cannot_be_injected_through_a_path_containing_newlines) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path victim = MakeProject(dir, "Victim");
    std::string evilName = "evil\n" + TrustStore::KeyFor(victim.string());
    std::error_code ec;
    fs::path evil = dir.path / evilName;
    fs::create_directories(evil, ec);
    if (ec) {
        std::printf("    note: this file system cannot create folders with newlines in their names\n");
        return;
    }
    CHECK(!store.Trust(evil.string()));
    CHECK(!store.IsTrusted(victim.string()));
    CHECK(store.TrustedKeys().empty());
}

TEST(trust_file_tolerates_hand_edits_and_rejects_corruption) {
    TempDir dir;
    TrustStore store(dir.Str("home"));
    fs::path project = MakeProject(dir, "A");
    std::string key = TrustStore::KeyFor(project.string());
    REQUIRE(!key.empty());

    REQUIRE(test::WriteText(dir.Path("home") / "trusted_projects.cfg", "StarEngineTrust1\r\n\r\n" + key + "\r\nsome\tcontrol\x01line\r\n"));
    CHECK(store.IsTrusted(project.string()));
    CHECK(store.TrustedKeys().size() == 1);

    REQUIRE(test::WriteText(dir.Path("home") / "trusted_projects.cfg", key + "\n"));
    CHECK(!store.IsTrusted(project.string()));

    REQUIRE(test::WriteText(dir.Path("home") / "trusted_projects.cfg", "StarEngineTrust2\n" + key + "\n"));
    CHECK(!store.IsTrusted(project.string()));

    REQUIRE(test::WriteFile(dir.Path("home") / "trusted_projects.cfg", std::vector<uint8_t>(10, 0)));
    CHECK(!store.IsTrusted(project.string()));

    REQUIRE(test::WriteText(dir.Path("home") / "trusted_projects.cfg", ""));
    CHECK(!store.IsTrusted(project.string()));

    std::string huge = "StarEngineTrust1\n" + key + "\n" + std::string(2 * 1024 * 1024, 'x') + "\n";
    REQUIRE(test::WriteText(dir.Path("home") / "trusted_projects.cfg", huge));
    CHECK(!store.IsTrusted(project.string()));

    CHECK(store.Trust(project.string()));
    CHECK(store.IsTrusted(project.string()));
}

TEST(trust_survives_a_missing_or_unwritable_store_directory) {
    TempDir dir;
    fs::path project = MakeProject(dir, "A");
    REQUIRE(test::WriteText(dir.Path("blocker"), "a file where a directory is expected"));
    TrustStore store(dir.Str("blocker"));
    CHECK(!store.IsTrusted(project.string()));
    CHECK(!store.Trust(project.string()));
    CHECK(!store.IsTrusted(project.string()));

    TrustStore fresh(dir.Str("does/not/exist/yet"));
    CHECK(fresh.Trust(project.string()));
    CHECK(fresh.IsTrusted(project.string()));
}

TEST(machine_token_is_random_hex_and_stable) {
    TempDir dir;
    TrustStore first(dir.Str("home"));
    std::string token = first.MachineToken();
    CHECK(IsHex64(token));
    CHECK(first.MachineToken() == token);

    TrustStore second(dir.Str("home"));
    CHECK(second.MachineToken() == token);

    TrustStore elsewhere(dir.Str("other_home"));
    CHECK(elsewhere.MachineToken() != token);
    CHECK(IsHex64(elsewhere.MachineToken()));
}

TEST(machine_token_is_regenerated_when_the_file_is_damaged) {
    TempDir dir;
    TrustStore first(dir.Str("home"));
    std::string token = first.MachineToken();

    for (const std::string& damaged : { std::string(""), std::string("not a token"), token.substr(1), token + "00", std::string(64, 'Z') }) {
        REQUIRE(test::WriteText(dir.Path("home") / "machine_id", damaged));
        TrustStore store(dir.Str("home"));
        std::string regenerated = store.MachineToken();
        CHECK(IsHex64(regenerated));
        CHECK(regenerated != token);
        TrustStore again(dir.Str("home"));
        CHECK(again.MachineToken() == regenerated);
        token = regenerated;
    }
}

TEST(machine_tokens_do_not_repeat) {
    TempDir dir;
    std::set<std::string> seen;
    for (int i = 0; i < 40; i++) {
        TrustStore store(dir.Str("home" + std::to_string(i)));
        seen.insert(store.MachineToken());
    }
    CHECK(seen.size() == 40);
}

TEST(build_cache_is_created_inside_the_project_with_a_marker) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    CHECK(!prep.wiped);
    CHECK(prep.error.empty());
    CHECK(fs::is_directory(project / "Saved" / "Build" / "Linux_Hardware"));

    CHECK(fs::equivalent(prep.path, project / "Saved" / "Build" / "Linux_Hardware"));
    CHECK(fs::is_regular_file(fs::path(prep.path) / BuildCacheGuard::MarkerFileName()));
}

TEST(build_cache_is_kept_when_the_marker_matches) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation first = BuildCacheGuard::Prepare(project.string(), "Windows_Hardware", kToken);
    REQUIRE(first.ok);
    REQUIRE(test::WriteText(fs::path(first.path) / "CMakeCache.txt", "cache"));
    REQUIRE(test::WriteText(fs::path(first.path) / "sub" / "object.o", "object"));

    BuildCacheGuard::Preparation second = BuildCacheGuard::Prepare(project.string(), "Windows_Hardware", kToken);
    REQUIRE(second.ok);
    CHECK(!second.wiped);
    CHECK(first.path == second.path);
    CHECK(fs::exists(fs::path(second.path) / "CMakeCache.txt"));
    CHECK(fs::exists(fs::path(second.path) / "sub" / "object.o"));
}

TEST(build_cache_from_another_machine_is_wiped) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation first = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(first.ok);
    REQUIRE(test::WriteText(fs::path(first.path) / "CMakeCache.txt", "cache"));

    BuildCacheGuard::Preparation second = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kOtherToken);
    REQUIRE(second.ok);
    CHECK(second.wiped);
    CHECK(!fs::exists(fs::path(second.path) / "CMakeCache.txt"));
    CHECK(fs::exists(fs::path(second.path) / BuildCacheGuard::MarkerFileName()));

    BuildCacheGuard::Preparation third = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kOtherToken);
    REQUIRE(third.ok);
    CHECK(!third.wiped);
}

TEST(preseeded_cache_without_a_marker_is_wiped) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Downloaded");
    fs::path cache = project / "Saved" / "Build" / "Linux_Hardware";
    REQUIRE(test::WriteText(cache / "CMakeCache.txt", "CMAKE_HOME_DIRECTORY:INTERNAL=/somewhere"));
    REQUIRE(test::WriteText(cache / "evil.cmake", "execute_process(COMMAND malicious)"));
    REQUIRE(test::WriteText(cache / "nested" / "deep" / "script.sh", "#!/bin/sh"));

    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    CHECK(prep.wiped);
    CHECK(!fs::exists(cache / "CMakeCache.txt"));
    CHECK(!fs::exists(cache / "evil.cmake"));
    CHECK(!fs::exists(cache / "nested"));
}

TEST(forged_or_copied_markers_are_rejected) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation a = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(a.ok);
    std::string markerOfA = test::ReadText(fs::path(a.path) / BuildCacheGuard::MarkerFileName());

    fs::path cacheB = project / "Saved" / "Build" / "Windows_Hardware";
    REQUIRE(test::WriteText(cacheB / BuildCacheGuard::MarkerFileName(), markerOfA));
    REQUIRE(test::WriteText(cacheB / "CMakeCache.txt", "seeded"));
    BuildCacheGuard::Preparation b = BuildCacheGuard::Prepare(project.string(), "Windows_Hardware", kToken);
    REQUIRE(b.ok);
    CHECK(b.wiped);
    CHECK(!fs::exists(cacheB / "CMakeCache.txt"));

    for (const std::string& forged : { std::string("StarEngineBuildCache1\n"), kToken, std::string("StarEngineBuildCache1\n" + std::string(64, '0') + "\n"), std::string(4000, 'x'), std::string("") }) {
        BuildCacheGuard::Preparation c = BuildCacheGuard::Prepare(project.string(), "Forge_Test", kToken);
        REQUIRE(c.ok);
        REQUIRE(test::WriteText(fs::path(c.path) / BuildCacheGuard::MarkerFileName(), forged));
        REQUIRE(test::WriteText(fs::path(c.path) / "payload.txt", "x"));
        BuildCacheGuard::Preparation d = BuildCacheGuard::Prepare(project.string(), "Forge_Test", kToken);
        REQUIRE(d.ok);
        CHECK(d.wiped);
        CHECK(!fs::exists(fs::path(d.path) / "payload.txt"));
    }
}

TEST(marker_does_not_reveal_the_machine_token) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    std::string marker = test::ReadText(fs::path(prep.path) / BuildCacheGuard::MarkerFileName());
    CHECK(marker.find(kToken) == std::string::npos);
    CHECK(marker.rfind("StarEngineBuildCache1\n", 0) == 0);
}

TEST(a_moved_project_gets_a_fresh_cache) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation first = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(first.ok);
    REQUIRE(test::WriteText(fs::path(first.path) / "CMakeCache.txt", "cache"));

    fs::path moved = dir.Path("MovedGame");
    std::error_code ec;
    fs::rename(project, moved, ec);
    REQUIRE(!ec);
    BuildCacheGuard::Preparation second = BuildCacheGuard::Prepare(moved.string(), "Linux_Hardware", kToken);
    REQUIRE(second.ok);
    CHECK(second.wiped);
    CHECK(!fs::exists(fs::path(second.path) / "CMakeCache.txt"));
}

TEST(different_cache_names_are_independent) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation a = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    BuildCacheGuard::Preparation b = BuildCacheGuard::Prepare(project.string(), "Linux_Software", kToken);
    REQUIRE(a.ok && b.ok);
    REQUIRE(test::WriteText(fs::path(a.path) / "a.txt", "a"));
    REQUIRE(test::WriteText(fs::path(b.path) / "b.txt", "b"));

    BuildCacheGuard::Preparation wipeB = BuildCacheGuard::Prepare(project.string(), "Linux_Software", kOtherToken);
    REQUIRE(wipeB.ok);
    CHECK(wipeB.wiped);
    CHECK(fs::exists(fs::path(a.path) / "a.txt"));
    CHECK(!fs::exists(fs::path(b.path) / "b.txt"));
}

TEST(invalid_cache_names_and_projects_are_refused) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    for (const char* name : { "", ".", "..", "a/b", "a\\b", "../escape", "C:x", "bad\nname" }) {
        BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), name, kToken);
        CHECK_MSG(!prep.ok, std::string("name: ") + name);
        CHECK(!prep.error.empty());
    }
    CHECK(!fs::exists(project / "Saved"));
    CHECK(!fs::exists(dir.Path("escape")));

    CHECK(!BuildCacheGuard::Prepare("", "Linux_Hardware", kToken).ok);
    CHECK(!BuildCacheGuard::Prepare(dir.Str("not_there"), "Linux_Hardware", kToken).ok);
    CHECK(!BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", "").ok);

    REQUIRE(test::WriteText(dir.Path("a_file"), "x"));
    CHECK(!BuildCacheGuard::Prepare(dir.Str("a_file"), "Linux_Hardware", kToken).ok);
}

TEST(build_cache_refuses_symbolic_links_that_leave_the_project) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    fs::path outside = MakeProject(dir, "Outside");
    REQUIRE(test::WriteText(outside / "Linux_Hardware" / "precious.txt", "do not delete"));
    REQUIRE(test::WriteText(outside / "precious_root.txt", "do not delete"));

    if (!MakeSymlink(outside, project / "Saved")) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }
    BuildCacheGuard::Preparation viaSaved = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    CHECK(!viaSaved.ok);
    CHECK(fs::exists(outside / "Linux_Hardware" / "precious.txt"));
    CHECK(fs::exists(outside / "precious_root.txt"));
    CHECK(!fs::exists(outside / "Build"));

    std::error_code ec;
    fs::remove(project / "Saved", ec);
    fs::create_directories(project / "Saved", ec);
    REQUIRE(MakeSymlink(outside, project / "Saved" / "Build"));
    BuildCacheGuard::Preparation viaBuild = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    CHECK(!viaBuild.ok);
    CHECK(fs::exists(outside / "Linux_Hardware" / "precious.txt"));

    fs::remove(project / "Saved" / "Build", ec);
    fs::create_directories(project / "Saved" / "Build", ec);
    REQUIRE(MakeSymlink(outside / "Linux_Hardware", project / "Saved" / "Build" / "Linux_Hardware"));
    BuildCacheGuard::Preparation viaCache = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    CHECK(!viaCache.ok);
    CHECK(fs::exists(outside / "Linux_Hardware" / "precious.txt"));
}

TEST(a_marker_that_is_a_symbolic_link_cannot_redirect_the_write) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    fs::path outside = MakeProject(dir, "Outside");
    REQUIRE(test::WriteText(outside / "precious.txt", "do not overwrite"));

    fs::path cache = project / "Saved" / "Build" / "Linux_Hardware";
    std::error_code ec;
    fs::create_directories(cache, ec);
    fs::create_symlink(outside / "precious.txt", cache / BuildCacheGuard::MarkerFileName(), ec);
    if (ec) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }

    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    CHECK(test::ReadText(outside / "precious.txt") == "do not overwrite");
    CHECK(!fs::is_symlink(cache / BuildCacheGuard::MarkerFileName()));
    CHECK(fs::is_regular_file(cache / BuildCacheGuard::MarkerFileName()));
}

TEST(reset_clears_the_cache_but_keeps_it_valid) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    REQUIRE(test::WriteText(fs::path(prep.path) / "CMakeCache.txt", "stale"));
    REQUIRE(test::WriteText(fs::path(prep.path) / "deep" / "x.o", "stale"));

    std::string error;
    CHECK(BuildCacheGuard::Reset(prep.path, kToken, error));
    CHECK(error.empty());
    CHECK(!fs::exists(fs::path(prep.path) / "CMakeCache.txt"));
    CHECK(!fs::exists(fs::path(prep.path) / "deep"));

    BuildCacheGuard::Preparation again = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(again.ok);
    CHECK(!again.wiped);

    CHECK(!BuildCacheGuard::Reset(dir.Str("not_there"), kToken, error));
    CHECK(!BuildCacheGuard::Reset(prep.path, "", error));
}

TEST(wiping_never_follows_symbolic_links_inside_the_cache) {
    TempDir dir;
    fs::path project = MakeProject(dir, "Game");
    fs::path outside = MakeProject(dir, "Outside");
    REQUIRE(test::WriteText(outside / "precious.txt", "do not delete"));

    fs::path cache = project / "Saved" / "Build" / "Linux_Hardware";
    std::error_code ec;
    fs::create_directories(cache, ec);
    if (!MakeSymlink(outside, cache / "link_out")) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }
    REQUIRE(test::WriteText(cache / "CMakeCache.txt", "seeded"));

    BuildCacheGuard::Preparation prep = BuildCacheGuard::Prepare(project.string(), "Linux_Hardware", kToken);
    REQUIRE(prep.ok);
    CHECK(prep.wiped);
    CHECK(!fs::exists(cache / "CMakeCache.txt"));
    CHECK(!fs::is_symlink(cache / "link_out"));
    CHECK(fs::exists(outside / "precious.txt"));
}

TEST_MAIN()
