#include "PackagePlatform.h"
#include "SafeProcess.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace {

bool CommandExists(const std::string& name) {
    return !SafeProcess::FindExecutableInPath(name).empty();
}

#ifdef _WIN32
std::atomic<bool> wslToolsConfirmed{false};

bool WslHasCommands(const std::vector<std::string>& names) {
    std::vector<std::string> argv = { "wsl", "which" };
    argv.insert(argv.end(), names.begin(), names.end());
    std::string log;
    return SafeProcess::Run(argv, log) == 0;
}
#endif

std::vector<PackagePlatformInfo> BuildPlatformList([[maybe_unused]] bool runSlowChecks) {
    const std::string pendingReason = "Checking available build tools...";
    auto has = [](const char* name) { return CommandExists(name); };

    std::vector<PackagePlatformInfo> list;

    PackagePlatformInfo linuxInfo;
    linuxInfo.platform = PackagePlatform::Linux;
    linuxInfo.name = "Linux";
    linuxInfo.displayName = "Linux (x86_64)";
#if defined(__linux__)
    linuxInfo.available = has("g++") && has("make") && has("cmake");
    if (!linuxInfo.available) {
        linuxInfo.unavailableReason =
            "Requires the Linux build tools (g++, make, cmake). "
            "Install them, e.g. \"sudo apt install build-essential cmake\" on Debian/Ubuntu.";
    }
#elif defined(_WIN32)
    if (!CommandExists("wsl")) {
        wslToolsConfirmed.store(false);
        linuxInfo.available = false;
        linuxInfo.unavailableReason = "WSL (Windows Subsystem for Linux) is not installed. "
            "Install WSL, then install build-essential, gcc, g++, make, and cmake inside it.";
    } else if (wslToolsConfirmed.load()) {
        linuxInfo.available = true;
    } else if (!runSlowChecks) {
        linuxInfo.available = false;
        linuxInfo.unavailableReason = pendingReason;
    } else if (!WslHasCommands({ "x86_64-linux-gnu-g++", "cmake" })) {
        linuxInfo.available = false;
        linuxInfo.unavailableReason = "WSL is installed but the Linux build tools are missing inside it. "
            "Inside WSL run: sudo apt install build-essential cmake";
    } else {
        wslToolsConfirmed.store(true);
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
    winInfo.available = has("x86_64-w64-mingw32-g++");
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
    macInfo.available = has("o64-clang") || has("oa64-clang");
    if (!macInfo.available) macInfo.unavailableReason = "Requires an osxcross toolchain with the macOS SDK.";
#endif
    list.push_back(macInfo);

    PackagePlatformInfo webInfo;
    webInfo.platform = PackagePlatform::Web;
    webInfo.name = "Web";
    webInfo.displayName = "Web (HTML5 / WASM)";
    webInfo.available = has("emcc");
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
    return BuildPlatformList(true);
}

std::vector<PackagePlatformInfo> GetPendingPackagePlatforms() {
    return BuildPlatformList(false);
}

std::shared_ptr<PackagePlatformProbe> StartPackagePlatformProbe() {
#ifdef _WIN32
    static std::mutex inFlightMutex;
    static std::weak_ptr<PackagePlatformProbe> inFlight;

    std::lock_guard<std::mutex> lock(inFlightMutex);
    if (std::shared_ptr<PackagePlatformProbe> running = inFlight.lock()) {
        if (!running->ready.load(std::memory_order_acquire)) return running;
    }

    auto state = std::make_shared<PackagePlatformProbe>();
    if (wslToolsConfirmed.load()) {
        state->platforms = BuildPlatformList(true);
        state->ready.store(true, std::memory_order_release);
        return state;
    }
    inFlight = state;
    std::thread([state]() {
        state->platforms = BuildPlatformList(true);
        state->ready.store(true, std::memory_order_release);
    }).detach();
#else
    auto state = std::make_shared<PackagePlatformProbe>();
    state->platforms = BuildPlatformList(true);
    state->ready.store(true, std::memory_order_release);
#endif
    return state;
}
