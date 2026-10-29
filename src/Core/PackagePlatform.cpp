#include "PackagePlatform.h"
#include <cstdlib>

namespace {

bool CommandExists(const std::string& name) {
#ifdef _WIN32
    std::string cmd = "where.exe " + name + " >nul 2>nul";
#else
    std::string cmd = "which " + name + " >/dev/null 2>&1";
#endif
    return system(cmd.c_str()) == 0;
}

#ifdef _WIN32
bool WslCommandExists(const std::string& name) {
    std::string cmd = "wsl which " + name + " >nul 2>nul";
    return system(cmd.c_str()) == 0;
}
#endif

} // namespace

std::string PackagePlatformName(PackagePlatform platform) {
    switch (platform) {
        case PackagePlatform::Windows: return "Windows";
        case PackagePlatform::Linux:   return "Linux";
        case PackagePlatform::macOS:   return "macOS";
        case PackagePlatform::Web:     return "Web";
        case PackagePlatform::Android: return "Android";
    }
    return "Linux";
}

PackagePlatform PackagePlatformFromName(const std::string& name) {
    if (name == "Windows") return PackagePlatform::Windows;
    if (name == "macOS")   return PackagePlatform::macOS;
    if (name == "Web")     return PackagePlatform::Web;
    if (name == "Android") return PackagePlatform::Android;
    return PackagePlatform::Linux;
}

std::vector<PackagePlatformInfo> GetAvailablePackagePlatforms() {
    std::vector<PackagePlatformInfo> list;

    PackagePlatformInfo linuxInfo;
    linuxInfo.platform = PackagePlatform::Linux;
    linuxInfo.name = "Linux";
    linuxInfo.displayName = "Linux (x86_64)";
#if defined(__linux__)
    linuxInfo.available = CommandExists("g++") && CommandExists("make") && CommandExists("cmake");
    if (!linuxInfo.available) {
        linuxInfo.unavailableReason = "Requires the Linux build tools (g++, make, cmake). "
            "Install them, e.g. \"sudo apt install build-essential cmake\" on Debian/Ubuntu.";
    }
#elif defined(_WIN32)
    if (!CommandExists("wsl")) {
        linuxInfo.available = false;
        linuxInfo.unavailableReason = "WSL (Windows Subsystem for Linux) is not installed. "
            "Install WSL, then install build-essential, gcc, g++, make, and cmake inside it.";
    } else if (!WslCommandExists("x86_64-linux-gnu-g++") || !WslCommandExists("cmake")) {
        linuxInfo.available = false;
        linuxInfo.unavailableReason = "WSL is installed but the Linux build tools are missing inside it. "
            "Inside WSL run: sudo apt install build-essential cmake";
    } else {
        linuxInfo.available = true;
    }
#else
    linuxInfo.available = false;
    linuxInfo.unavailableReason = "Packaging for Linux from this platform is not supported yet.";
#endif
    list.push_back(linuxInfo);

    PackagePlatformInfo winInfo;
    winInfo.platform = PackagePlatform::Windows;
    winInfo.name = "Windows";
    winInfo.displayName = "Windows (x86_64)";
#if defined(_WIN32)
    winInfo.available = true;
#else
    winInfo.available = CommandExists("x86_64-w64-mingw32-g++");
    if (!winInfo.available) winInfo.unavailableReason = "Requires the mingw-w64 cross toolchain.";
#endif
    list.push_back(winInfo);

    PackagePlatformInfo macInfo;
    macInfo.platform = PackagePlatform::macOS;
    macInfo.name = "macOS";
    macInfo.displayName = "macOS (x86_64 / arm64)";
#if defined(__APPLE__)
    macInfo.available = true;
#else
    macInfo.available = CommandExists("o64-clang") || CommandExists("oa64-clang");
    if (!macInfo.available) macInfo.unavailableReason = "Requires an osxcross toolchain with the macOS SDK.";
#endif
    list.push_back(macInfo);

    PackagePlatformInfo webInfo;
    webInfo.platform = PackagePlatform::Web;
    webInfo.name = "Web";
    webInfo.displayName = "Web (HTML5 / WASM)";
    webInfo.available = CommandExists("emcc");
    if (!webInfo.available) webInfo.unavailableReason = "Requires the Emscripten SDK (emcc not found on PATH).";
    list.push_back(webInfo);

    PackagePlatformInfo androidInfo;
    androidInfo.platform = PackagePlatform::Android;
    androidInfo.name = "Android";
    androidInfo.displayName = "Android (arm64)";
    androidInfo.available = false;
    androidInfo.unavailableReason = "APK packaging is not implemented yet.";
    list.push_back(androidInfo);

    return list;
}
