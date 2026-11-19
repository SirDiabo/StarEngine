#include "EngineRoot.h"
#include "ExecutablePath.h"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace EngineRoot {

bool IsInsideAppBundle(const std::string& exePath) {
    if (exePath.empty()) return false;
    fs::path macosDir = fs::path(exePath).parent_path();
    fs::path contentsDir = macosDir.parent_path();
    return macosDir.filename() == "MacOS" && contentsDir.filename() == "Contents" && contentsDir.parent_path().extension() == ".app";
}

std::vector<std::string> ExternalPakCandidates(const std::string& exePath) {
    std::vector<std::string> out;
    if (exePath.empty()) return out;
    fs::path exeDir = fs::path(exePath).parent_path();
    out.push_back((exeDir / "StarEngine.pak").string());
    if (IsInsideAppBundle(exePath)) out.push_back((exeDir.parent_path() / "Resources" / "StarEngine.pak").string());
    return out;
}

std::string Resolve() {
    if (const char* envRoot = std::getenv("STARENGINE_ENGINE_ROOT")) {
        if (envRoot[0] != '\0') return envRoot;
    }

    std::string exePath = GetExecutablePath();
    if (!exePath.empty()) {
        fs::path shipped = fs::path(exePath).parent_path() / "Source";
        if (fs::exists(shipped / "CMakeLists.txt")) return shipped.string();
        if (IsInsideAppBundle(exePath)) {
            fs::path bundled = fs::path(exePath).parent_path().parent_path() / "Resources" / "Source";
            if (fs::exists(bundled / "CMakeLists.txt")) return bundled.string();
        }
    }

#ifdef STARENGINE_SOURCE_ROOT
    fs::path devRoot = fs::path(STARENGINE_SOURCE_ROOT).parent_path();
    if (fs::exists(devRoot / "CMakeLists.txt")) return devRoot.string();
#endif

    return fs::current_path().string();
}

std::string BuildDirectory() {
    std::string exePath = GetExecutablePath();
    if (IsInsideAppBundle(exePath)) {
        const char* home = std::getenv("HOME");
        fs::path base = (home && home[0] != '\0') ? fs::path(home) / "Library" / "Application Support" / "StarEngine" : fs::temp_directory_path() / "StarEngine";
        return (base / "Binaries" / "Build").string();
    }
    fs::path anchor = !exePath.empty() ? fs::path(exePath).parent_path() : fs::current_path();
    return (anchor / "Binaries" / "Build").string();
}

}
