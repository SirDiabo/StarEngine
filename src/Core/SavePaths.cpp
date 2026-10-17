#include "SavePaths.h"
#include "Project.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {
    std::string packagedGameName;
    std::string currentSlot;

    bool IsReservedDeviceName(const std::string& name) {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        size_t dot = upper.find('.');
        if (dot != std::string::npos) upper.resize(dot);
        if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") return true;
        if (upper.size() == 4 && (upper.compare(0, 3, "COM") == 0 || upper.compare(0, 3, "LPT") == 0)) return upper[3] >= '1' && upper[3] <= '9';
        return false;
    }

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
    return GetSaveFilePathInSlot(currentSlot, saveFileName);
}

std::string SavePaths::NormalizeSlotName(const std::string& raw) {
    size_t first = raw.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = raw.find_last_not_of(" \t");
    std::string name = raw.substr(first, last - first + 1);

    for (char& c : name) {
        unsigned char u = (unsigned char)c;
        bool allowed = std::isalnum(u) || c == '_' || c == '-' || c == '.' || c == ' ';
        if (!allowed) c = '_';
    }
    if (name.size() > 64) name.resize(64);
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    if (name.empty()) return "";
    if (name[0] == '.') name[0] = '_';
    if (IsReservedDeviceName(name)) name += "_";
    return name;
}

void SavePaths::SetCurrentSlot(const std::string& slot) {
    currentSlot = NormalizeSlotName(slot);
}

const std::string& SavePaths::GetCurrentSlot() {
    return currentSlot;
}

std::string SavePaths::GetSlotsRoot() {
    return (fs::path(GetSaveDirectory()) / "Slots").string();
}

std::string SavePaths::GetSlotDirectory(const std::string& slot) {
    std::string normalized = NormalizeSlotName(slot);
    if (normalized.empty()) return GetSaveDirectory();
    return (fs::path(GetSlotsRoot()) / normalized).string();
}

std::string SavePaths::GetSaveFilePathInSlot(const std::string& slot, const std::string& saveFileName) {
    return (fs::path(GetSlotDirectory(slot)) / (saveFileName + ".starsav")).string();
}
