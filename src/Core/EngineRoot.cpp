#include "EngineRoot.h"
#include "ExecutablePath.h"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace EngineRoot {

std::string Resolve() {
    if (const char* envRoot = std::getenv("STARENGINE_ENGINE_ROOT")) {
        if (envRoot[0] != '\0') return envRoot;
    }

    std::string exePath = GetExecutablePath();
    if (!exePath.empty()) {
        fs::path shipped = fs::path(exePath).parent_path() / "Source";
        if (fs::exists(shipped / "CMakeLists.txt")) return shipped.string();
    }

#ifdef STARENGINE_SOURCE_ROOT
    fs::path devRoot = fs::path(STARENGINE_SOURCE_ROOT).parent_path();
    if (fs::exists(devRoot / "CMakeLists.txt")) return devRoot.string();
#endif

    return fs::current_path().string();
}

std::string BuildDirectory() {
    std::string exePath = GetExecutablePath();
    fs::path anchor = !exePath.empty() ? fs::path(exePath).parent_path() : fs::current_path();
    return (anchor / "Binaries" / "Build").string();
}

}
