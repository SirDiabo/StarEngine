#include "PackageRunner.h"
#include "PackageCooker.h"
#include "StarPak.h"
#include "SafeProcess.h"
#include "EngineRoot.h"
#include "CMakeBuild.h"
#include "BuildDiagnostics.h"
#include "Runtime/PackagedGameConfig.h"
#include "RenderSettings.h"
#include "ShadingTechniqueRegistry.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <thread>

std::string PackageFormatName(PackageFormat format) {
    return format == PackageFormat::LooseFolder ? "LooseFolder" : "SingleFile";
}

PackageFormat PackageFormatFromName(const std::string& name) {
    return name == "LooseFolder" ? PackageFormat::LooseFolder : PackageFormat::SingleFile;
}

namespace fs = std::filesystem;

namespace {

std::string GetEngineRoot() {
    return EngineRoot::Resolve();
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

std::string RewriteToContentRelative(const std::string& mapPath, const fs::path& contentRoot) {
    if (mapPath.empty()) return mapPath;
    if (!fs::path(mapPath).is_absolute()) return mapPath;

    std::error_code ec;
    fs::path relative = fs::relative(fs::path(mapPath), contentRoot, ec);
    if (ec || relative.generic_string().rfind("..", 0) == 0) return mapPath;
    return relative.generic_string();
}

} // namespace

PackageResult PackageRunner::Package(const PackageOptions& options,
                                      const std::function<void(const std::string&)>& onOutput) {
    PackageResult result;

    if (options.platform == PackagePlatform::Android) {
        result.error = "Android packaging is not implemented yet.";
        return result;
    }

    fs::path engineRoot = GetEngineRoot();
    if (!fs::exists(engineRoot / "CMakeLists.txt")) {
        result.error = "Could not locate the engine source tree to build from (looked in \"" + engineRoot.string() +
            "\"). Packaging compiles your game against the engine source, so it must be present on disk. "
            "If you built the editor yourself this should be automatic; if you downloaded a prebuilt editor, "
            "grab the matching engine source and point to it with the STARENGINE_ENGINE_ROOT environment variable.";
        return result;
    }

    std::string cmakeMissing = BuildDiagnostics::CheckCMakeAvailable();
    if (!cmakeMissing.empty()) {
        result.error = cmakeMissing;
        return result;
    }

    std::string platformKey = PackagePlatformName(options.platform);
    std::string rendererKey = GameRendererName(options.renderer);
    fs::path buildDir = fs::temp_directory_path() / "StarEnginePackageBuild" / (platformKey + "_" + rendererKey);

    if (CMakeBuild::CacheSourceMismatch(buildDir.string(), engineRoot.string())) {
        std::error_code wipeEc;
        fs::remove_all(buildDir, wipeEc);
    }

    std::string toolchainFile = WriteToolchainFileIfNeeded(options.platform, buildDir.string());

    bool isWeb = options.platform == PackagePlatform::Web;

    std::vector<std::string> configureArgs;
    if (isWeb) configureArgs.push_back("emcmake");
    configureArgs.push_back("cmake");
    configureArgs.push_back("-S");
    configureArgs.push_back(engineRoot.string());
    configureArgs.push_back("-B");
    configureArgs.push_back(buildDir.string());
    configureArgs.push_back("-DSTARENGINE_BUILD_GAME=ON");
    configureArgs.push_back("-DCMAKE_BUILD_TYPE=Release");
    if (!toolchainFile.empty()) {
        configureArgs.push_back("-DCMAKE_TOOLCHAIN_FILE=" + toolchainFile);
    }
    if (isWeb) configureArgs.push_back("-DPLATFORM=Web");
    else if (options.renderer == GameRenderer::Software) {
        configureArgs.push_back("-DPLATFORM=RGFW");
        configureArgs.push_back("-DOPENGL_VERSION=Software");
    }

    result.log += "== Configure ==\n";
    for (const auto& a : configureArgs) result.log += a + " ";
    result.log += "\n";
    if (onOutput) onOutput("== Configure ==\n");
    if (SafeProcess::Run(configureArgs, result.log, onOutput) != 0) {
        result.error = "CMake configure failed. See log.";
        std::string hint = BuildDiagnostics::DiagnoseConfigureFailure(result.log);
        if (!hint.empty()) result.error += " " + hint;
        return result;
    }

    unsigned int jobs = std::thread::hardware_concurrency();
    if (jobs == 0) jobs = 2;
    std::vector<std::string> buildArgs;
    if (isWeb) buildArgs.push_back("emmake");
    buildArgs.push_back("cmake");
    buildArgs.push_back("--build");
    buildArgs.push_back(buildDir.string());
    buildArgs.push_back("--config");
    buildArgs.push_back("Release");
    buildArgs.push_back("-j" + std::to_string(jobs));

    result.log += "\n== Build ==\n";
    for (const auto& a : buildArgs) result.log += a + " ";
    result.log += "\n";
    if (onOutput) onOutput("\n== Build ==\n");
    if (SafeProcess::Run(buildArgs, result.log, onOutput) != 0) {
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

    const ProjectSettings& settings = Project::Get().GetSettings();
    if (!settings.name.empty()) {
        std::string renamedFilename = settings.name;
        if (options.platform == PackagePlatform::Windows) renamedFilename += ".exe";
        fs::path renamedPath = fs::path(options.outputDir) / renamedFilename;
        if (renamedPath != outputExe) {
            std::error_code renameEc;
            fs::rename(outputExe, renamedPath, renameEc);
            if (!renameEc) outputExe = renamedPath;
        }
    }

    fs::path defaultIcon = engineRoot / "Assets" / "logo.png";
    if (fs::exists(defaultIcon)) {
        fs::copy_file(defaultIcon, fs::path(options.outputDir) / "icon.png",
                       fs::copy_options::overwrite_existing, ec);
        ec.clear();
    }

    fs::path contentRoot = fs::path(options.projectRoot) / "Content";
    std::string cipherKey = options.cipher == CookCipher::None ? "" : GenerateCipherKey();

    fs::path inputMappingsSource = fs::path(options.projectRoot) / "Saved" / "InputMappings.cfg";
    std::vector<uint8_t> inputMappingsBytes;
    if (fs::exists(inputMappingsSource)) {
        std::ifstream mappingsIn(inputMappingsSource, std::ios::binary);
        inputMappingsBytes.assign(std::istreambuf_iterator<char>(mappingsIn), std::istreambuf_iterator<char>());
    }

    PackagedGameConfig bootConfig;
    bootConfig.name = settings.name;
    bootConfig.renderScale = RenderSettings::Get().GetRenderScale();
    bootConfig.shadingType = RenderSettings::Get().GetShadingType();
    bootConfig.lightingQuality = LightingQualityName(RenderSettings::Get().GetLightingQuality());
    bootConfig.defaultMap = RewriteToContentRelative(settings.defaultMapPath, contentRoot);
    bootConfig.renderer = rendererKey;
    bootConfig.cipher = CookCipherName(options.cipher);
    bootConfig.cipherKey = cipherKey;

    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(bootConfig.shadingType);
    if (activeTechnique && activeTechnique->collectPackagedSettings) {
        bootConfig.moduleSettings = activeTechnique->collectPackagedSettings();
    }

    if (options.format == PackageFormat::SingleFile) {
        fs::path pakTempPath = buildDir / "content.starpak";

        std::vector<PackageCooker::ExtraPakFile> extraFiles;
        if (!inputMappingsBytes.empty()) {
            extraFiles.push_back({StarPak::kInputMappingsPath, inputMappingsBytes});
        }

        CookResult cookResult = PackageCooker::CookToPak(contentRoot.string(), pakTempPath.string(),
                                                            options.cipher, cipherKey, bootConfig.ToText(), extraFiles);
        if (!cookResult.success) {
            result.error = "Cooking failed: " + cookResult.error;
            return result;
        }
        result.log += "\n== Cook ==\nCooked " + std::to_string(cookResult.filesCooked) +
            " files into a single .starpak.\n";
        if (onOutput) onOutput("\n== Cook ==\nCooked " + std::to_string(cookResult.filesCooked) +
            " files into a single .starpak.\n");

        if (!StarPak::AppendPakToExecutable(outputExe.string(), pakTempPath.string())) {
            result.error = "Could not append pak data to the packaged executable.";
            return result;
        }
    } else {
        fs::path outputContentDir = fs::path(options.outputDir) / "Content";

        CookResult cookResult = PackageCooker::CookLoose(contentRoot.string(), outputContentDir.string(),
                                                            options.cipher, cipherKey);
        if (!cookResult.success) {
            result.error = "Cooking failed: " + cookResult.error;
            return result;
        }
        result.log += "\n== Cook ==\nCooked " + std::to_string(cookResult.filesCooked) + " files.\n";
        if (onOutput) onOutput("\n== Cook ==\nCooked " + std::to_string(cookResult.filesCooked) + " files.\n");

        if (!inputMappingsBytes.empty()) {
            fs::path outputMappingsPath = fs::path(options.outputDir) / "Saved" / "InputMappings.cfg";
            std::ofstream mappingsOut(outputMappingsPath, std::ios::binary);
            mappingsOut.write(reinterpret_cast<const char*>(inputMappingsBytes.data()), (std::streamsize)inputMappingsBytes.size());
        }

        fs::path bootstrapPath = fs::path(options.outputDir) / (settings.name + ".starproject");
        std::ofstream out(bootstrapPath);
        out << bootConfig.ToText();
    }

    result.success = true;
    result.outputExecutable = outputExe.string();
    return result;
}