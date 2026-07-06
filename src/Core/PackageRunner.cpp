#include "PackageRunner.h"
#include "PackageCooker.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

namespace fs = std::filesystem;

namespace {

std::string GetEngineRoot() {
#ifdef STARENGINE_SOURCE_ROOT
    return fs::path(STARENGINE_SOURCE_ROOT).parent_path().string();
#else
    return fs::current_path().string();
#endif
}

bool RunCommand(const std::string& cmd, std::string& log) {
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(cmd.c_str(), "r");
#else
    pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        log += "Failed to start: " + cmd + "\n";
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) log += buffer;

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif
    return exitCode == 0;
}

std::string GenerateCipherKey() {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, (int)sizeof(alphabet) - 2);
    std::string key(24, ' ');
    for (char& c : key) c = alphabet[dist(gen)];
    return key;
}

std::string WriteToolchainFileIfNeeded(PackagePlatform platform, const std::string& buildDir) {
    std::string content;

#if !defined(_WIN32)
    if (platform == PackagePlatform::Windows) {
        content =
            "set(CMAKE_SYSTEM_NAME Windows)\n"
            "set(CMAKE_SYSTEM_PROCESSOR x86_64)\n"
            "set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)\n"
            "set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)\n"
            "set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)\n"
            "set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)\n"
            "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n"
            "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n"
            "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n";
    }
#endif
#if !defined(__APPLE__)
    if (platform == PackagePlatform::macOS) {
        content =
            "set(CMAKE_SYSTEM_NAME Darwin)\n"
            "set(CMAKE_SYSTEM_PROCESSOR x86_64)\n"
            "set(CMAKE_C_COMPILER o64-clang)\n"
            "set(CMAKE_CXX_COMPILER o64-clang++)\n";
    }
#endif

    if (content.empty()) return "";

    fs::path path = fs::path(buildDir) / "toolchain.cmake";
    std::error_code ec;
    fs::create_directories(buildDir, ec);
    std::ofstream out(path);
    out << content;
    return path.string();
}

fs::path LocateBuiltExecutable(const fs::path& buildDir, PackagePlatform platform) {
    fs::path exe = buildDir / (platform == PackagePlatform::Windows ? "StarEngineGame.exe" : "StarEngineGame");
    if (fs::exists(exe)) return exe;

    for (const char* config : {"Release", "Debug"}) {
        fs::path nested = buildDir / config / exe.filename();
        if (fs::exists(nested)) return nested;
    }
    return {};
}

std::string RewriteToContentRelative(const std::string& absolutePath, const fs::path& contentRoot) {
    if (absolutePath.empty()) return absolutePath;
    std::error_code ec;
    fs::path relative = fs::relative(fs::path(absolutePath), contentRoot, ec);
    if (ec || relative.generic_string().rfind("..", 0) == 0) return absolutePath;
    return relative.generic_string();
}

} // namespace

PackageResult PackageRunner::Package(const PackageOptions& options) {
    PackageResult result;

    if (options.platform == PackagePlatform::Android) {
        result.error = "Android packaging is not implemented yet.";
        return result;
    }

    fs::path engineRoot = GetEngineRoot();
    if (!fs::exists(engineRoot / "CMakeLists.txt")) {
        result.error = "Could not locate the engine source tree to build from.";
        return result;
    }

    std::string platformKey = PackagePlatformName(options.platform);
    std::string rendererKey = GameRendererName(options.renderer);
    fs::path buildDir = fs::temp_directory_path() / "StarEnginePackageBuild" / (platformKey + "_" + rendererKey);

    std::string toolchainFile = WriteToolchainFileIfNeeded(options.platform, buildDir.string());

    bool isWeb = options.platform == PackagePlatform::Web;

    std::string configureCmd = isWeb ? "emcmake cmake" : "cmake";
    configureCmd += " -S \"" + engineRoot.string() + "\" -B \"" + buildDir.string() + "\"";
    configureCmd += " -DSTARENGINE_BUILD_GAME=ON -DCMAKE_BUILD_TYPE=Release";
    if (!toolchainFile.empty()) configureCmd += " -DCMAKE_TOOLCHAIN_FILE=\"" + toolchainFile + "\"";
    if (isWeb) configureCmd += " -DPLATFORM=Web";
    else if (options.renderer == GameRenderer::Software) configureCmd += " -DPLATFORM=RGFW -DOPENGL_VERSION=Software";
    configureCmd += " 2>&1";

    result.log += "== Configure ==\n" + configureCmd + "\n";
    if (!RunCommand(configureCmd, result.log)) {
        result.error = "CMake configure failed. See log.";
        return result;
    }

    unsigned int jobs = std::thread::hardware_concurrency();
    if (jobs == 0) jobs = 2;
    std::string buildCmd = (isWeb ? "emmake cmake" : "cmake");
    buildCmd += " --build \"" + buildDir.string() + "\" -j" + std::to_string(jobs) + " 2>&1";

    result.log += "\n== Build ==\n" + buildCmd + "\n";
    if (!RunCommand(buildCmd, result.log)) {
        result.error = "Build failed. See log.";
        return result;
    }

    if (isWeb) {
        
        result.success = true;
        result.outputExecutable = buildDir.string();
        return result;
    }

    fs::path exePath = LocateBuiltExecutable(buildDir, options.platform);
    if (exePath.empty()) {
        result.error = "Build succeeded but the game executable was not found.";
        return result;
    }

    std::error_code ec;
    fs::create_directories(options.outputDir, ec);
    fs::path outputExe = fs::path(options.outputDir) / exePath.filename();
    fs::copy_file(exePath, outputExe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "Could not copy executable to output folder: " + ec.message();
        return result;
    }

    fs::path contentRoot = fs::path(options.projectRoot) / "Content";
    fs::path outputContentDir = fs::path(options.outputDir) / "Content";
    std::string cipherKey = options.cipher == CookCipher::None ? "" : GenerateCipherKey();

    CookResult cookResult = PackageCooker::Cook(contentRoot.string(), outputContentDir.string(),
                                                  options.cipher, cipherKey);
    if (!cookResult.success) {
        result.error = "Cooking failed: " + cookResult.error;
        return result;
    }
    result.log += "\n== Cook ==\nCooked " + std::to_string(cookResult.filesCooked) + " files.\n";

    const ProjectSettings& settings = Project::Get().GetSettings();
    std::string relativeDefaultMap = RewriteToContentRelative(settings.defaultMapPath, contentRoot);

    fs::path bootstrapPath = fs::path(options.outputDir) / (settings.name + ".starproject");
    std::ofstream out(bootstrapPath);
    out << "name=" << settings.name << "\n";
    out << "targetWidth=" << settings.targetWidth << "\n";
    out << "targetHeight=" << settings.targetHeight << "\n";
    out << "defaultMap=" << relativeDefaultMap << "\n";
    out << "renderer=" << rendererKey << "\n";
    out << "cipher=" << CookCipherName(options.cipher) << "\n";
    out << "cipherKey=" << cipherKey << "\n";

    result.success = true;
    result.outputExecutable = outputExe.string();
    return result;
}
