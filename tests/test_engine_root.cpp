#include "TestHelper.h"
#include "Core/EngineRoot.h"

TEST(app_bundle_detection) {
    CHECK(EngineRoot::IsInsideAppBundle("/Applications/StarEngine.app/Contents/MacOS/StarEngine"));
    CHECK(EngineRoot::IsInsideAppBundle("/Users/me/Downloads/My Engine.app/Contents/MacOS/StarEngine"));
    CHECK(!EngineRoot::IsInsideAppBundle("/Applications/StarEngine.app/Contents/Resources/StarEngine"));
    CHECK(!EngineRoot::IsInsideAppBundle("/Applications/StarEngine/Contents/MacOS/StarEngine"));
    CHECK(!EngineRoot::IsInsideAppBundle("/home/me/engine/build/StarEngine"));
    CHECK(!EngineRoot::IsInsideAppBundle("/StarEngine"));
    CHECK(!EngineRoot::IsInsideAppBundle(""));
}

TEST(external_pak_candidates) {
    std::vector<std::string> plain = EngineRoot::ExternalPakCandidates("/home/me/engine/build/StarEngine");
    REQUIRE(plain.size() == 1);
    CHECK(plain[0].find("StarEngine.pak") != std::string::npos);
    CHECK(plain[0].find("Resources") == std::string::npos);

    std::vector<std::string> bundle = EngineRoot::ExternalPakCandidates("/Applications/StarEngine.app/Contents/MacOS/StarEngine");
    REQUIRE(bundle.size() == 2);
    CHECK(bundle[0].find("MacOS") != std::string::npos);
    CHECK(bundle[1].find("Contents") != std::string::npos);
    CHECK(bundle[1].find("Resources") != std::string::npos);
    CHECK(bundle[1].find("StarEngine.pak") != std::string::npos);

    CHECK(EngineRoot::ExternalPakCandidates("").empty());
}

TEST_MAIN()
