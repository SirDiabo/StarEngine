#include "SavePaths.h"
#include "Project.h"
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {
    std::string packagedGameName;

    std::string GetOSSaveRoot() {
        std::string base;
#ifdef _WIN32
        char* buf = nullptr;
        size_t len = 0;
        if (_dupenv_s(&buf, &len, "APPDATA") == 0 && buf) {
            base = buf;
            free(buf);
        }
#elif defined(__APPLE__)
        const char* home = getenv("HOME");
        if (home) base = (fs::path(home) / "Library" / "Application Support").string();
#else
        const char* xdg = getenv("XDG_DATA_HOME");
        if (xdg && *xdg) {
            base = xdg;
        } else {
            const char* home = getenv("HOME");
            if (home) base = (fs::path(home) / ".local" / "share").string();
        }
#endif
        if (base.empty()) base = ".";
        return base;
    }
}

void SavePaths::SetPackagedGameName(const std::string& name) {
    packagedGameName = name;
}

std::string SavePaths::GetSaveDirectory() {
    const std::string& root = Project::Get().GetRootPath();
    if (!root.empty()) {
        return (fs::path(root) / "Saved" / "SaveGames").string();
    }

    std::string gameName = packagedGameName.empty() ? "StarEngineGame" : packagedGameName;
    return (fs::path(GetOSSaveRoot()) / gameName / "SaveGames").string();
}

std::string SavePaths::GetSaveFilePath(const std::string& saveFileName) {
    return (fs::path(GetSaveDirectory()) / (saveFileName + ".starsav")).string();
}
