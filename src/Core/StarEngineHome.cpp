#include "StarEngineHome.h"
#include "EnvVar.h"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace StarEngineHome {

std::string Directory() {
    std::string home;
#ifdef _WIN32
    home = GetEnvironmentValue("USERPROFILE");
    if (home.empty()) home = ".";
#else
    const char* h = std::getenv("HOME");
    home = h ? h : ".";
#endif
    fs::path dir = fs::path(home) / ".starengine";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

}
