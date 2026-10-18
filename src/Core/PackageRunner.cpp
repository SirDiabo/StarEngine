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
#if defined(_WIN32)
    if (platform == PackagePlatform::Linux) {
        content =
            "set(CMAKE_SYSTEM_NAME Linux)\n"
            "set(CMAKE_SYSTEM_PROCESSOR x86_64)\n"
            "set(CMAKE_C_COMPILER x86_64-linux-gnu-gcc)\n"
            "set(CMAKE_CXX_COMPILER x86_64-linux-gnu-g++)\n";
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

#ifdef _WIN32
std::string ToWslPath(const std::string& winPath) {
    std::string p = winPath;
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    if (p.size() >= 2 && p[1] == ':') {
        char drive = p[0];
        if (drive >= 'A' && drive <= 'Z') drive = static_cast<char>(drive + ('a' - 'A'));
        p = "/mnt/" + std::string(1, drive) + p.substr(2);
    }
    return p;
}
#endif

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

#ifdef _WIN32
    bool useWsl = (options.platform == PackagePlatform::Linux);
#else
    bool useWsl = false;
#endif

    if (CMakeBuild::CacheSourceMismatch(buildDir.string(), engineRoot.string())) {
        std::error_code wipeEc;
        fs::remove_all(buildDir, wipeEc);
    }

    std::string toolchainFile = WriteToolchainFileIfNeeded(options.platform, buildDir.string());

    std::string wslEngineRoot = engineRoot.string();
    std::string wslBuildDir = buildDir.string();
    std::string wslToolchainFile = toolchainFile;
#ifdef _WIN32
    if (useWsl) {
        wslEngineRoot = ToWslPath(engineRoot.string());
        wslBuildDir = ToWslPath(buildDir.string());
        if (!toolchainFile.empty()) wslToolchainFile = ToWslPath(toolchainFile);
    }
#endif

    bool isWeb = options.platform == PackagePlatform::Web;

    std::vector<std::string> configureArgs;
    if (useWsl) configureArgs.push_back("wsl");
    if (isWeb) configureArgs.push_back("emcmake");
    configureArgs.push_back("cmake");
    configureArgs.push_back("-S");
    configureArgs.push_back(wslEngineRoot);
    configureArgs.push_back("-B");
    configureArgs.push_back(wslBuildDir);
    configureArgs.push_back("-DSTARENGINE_BUILD_GAME=ON");
    configureArgs.push_back("-DCMAKE_BUILD_TYPE=Release");
    if (!wslToolchainFile.empty()) {
        configureArgs.push_back("-DCMAKE_TOOLCHAIN_FILE=" + wslToolchainFile);
    }
    if (isWeb) configureArgs.push_back("-DPLATFORM=Web");
    if (isWeb) configureArgs.push_back("-DCMAKE_EXE_LINKER_FLAGS=-sALLOW_MEMORY_GROWTH=1 -sASYNCIFY=1 -sUSE_WEBGL2=1");
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
    if (useWsl) buildArgs.push_back("wsl");
    if (isWeb) buildArgs.push_back("emmake");
    buildArgs.push_back("cmake");
    buildArgs.push_back("--build");
    buildArgs.push_back(wslBuildDir);
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
        std::error_code ec;
        fs::create_directories(options.outputDir, ec);
        if (ec) {
            result.error = "Could not create output directory: " + ec.message();
            return result;
        }

        fs::path foundHtml;
        fs::path foundJs;
        fs::path foundWasm;
        for (const auto& entry : fs::directory_iterator(buildDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            if (ext == ".html" && foundHtml.empty()) foundHtml = entry.path();
            else if (ext == ".js" && foundJs.empty()) foundJs = entry.path();
            else if (ext == ".wasm" && foundWasm.empty()) foundWasm = entry.path();
        }

        if (foundHtml.empty() && foundJs.empty()) {
            result.error = "Build succeeded but no HTML/JS output was found in the build directory.";
            return result;
        }

        auto copyIfExists = [&](const fs::path& src, const fs::path& dst) {
            if (src.empty()) return;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            ec.clear();
        };

        fs::path outputHtml = fs::path(options.outputDir) / foundHtml.filename().string();
        copyIfExists(foundHtml, outputHtml);
        copyIfExists(foundJs, fs::path(options.outputDir) / foundJs.filename().string());
        copyIfExists(foundWasm, fs::path(options.outputDir) / foundWasm.filename().string());

        fs::path outputExe = !foundHtml.empty() ? outputHtml : fs::path(options.outputDir) / foundJs.filename().string();

        const ProjectSettings& settings = Project::Get().GetSettings();
        if (!settings.name.empty()) {
            if (!foundHtml.empty()) {
                std::string renamedFilename = settings.name + ".html";
                fs::path renamedPath = fs::path(options.outputDir) / renamedFilename;
                if (renamedPath != outputHtml) {
                    std::error_code renameEc;
                    fs::rename(outputHtml, renamedPath, renameEc);
                    if (!renameEc) outputExe = renamedPath;
                }
            } else if (!foundJs.empty()) {
                std::string htmlFilename = settings.name + ".html";
                fs::path generatedHtml = fs::path(options.outputDir) / htmlFilename;
                std::ofstream htmlOut(generatedHtml);
                htmlOut << "<!DOCTYPE html>\n<html>\n<head>\n  <meta charset=\"utf-8\">\n";
                htmlOut << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
                htmlOut << "  <title>" << settings.name << "</title>\n";
                htmlOut << "  <style>body{margin:0;padding:0;background:#000;overflow:hidden;}canvas{display:block;width:100vw;height:100vh;}</style>\n";
                htmlOut << "</head>\n<body>\n";
                htmlOut << "  <canvas id=\"canvas\"></canvas>\n";
                htmlOut << "  <script>var Module={canvas:document.getElementById('canvas')};</script>\n";
                htmlOut << "  <script src=\"" << foundJs.filename().string() << "\"></script>\n</body>\n</html>\n";
                outputHtml = generatedHtml;
                outputExe = generatedHtml;
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

        fs::path inputMappingsSource = fs::path(options.projectRoot) / "Config" / "InputMappings.cfg";
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
        bootConfig.defaultMap = RewriteToContentRelative(settings.gameDefaultMapPath, contentRoot);
        bootConfig.renderer = rendererKey;
        bootConfig.cipher = CookCipherName(options.cipher);
        bootConfig.cipherKey = cipherKey;

        const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(bootConfig.shadingType);
        if (activeTechnique && activeTechnique->collectPackagedSettings) {
            bootConfig.moduleSettings = activeTechnique->collectPackagedSettings();
        }

        if (options.format == PackageFormat::SingleFile) {
            fs::path pakTempPath = fs::path(options.outputDir) / "content.starpak";

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

            fs::path bootstrapPath = fs::path(options.outputDir) / (foundJs.stem().string() + ".starproject");
            std::ofstream out(bootstrapPath);
            out << bootConfig.ToText();
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
                fs::path outputMappingsPath = fs::path(options.outputDir) / "Config" / "InputMappings.cfg";
                std::ofstream mappingsOut(outputMappingsPath, std::ios::binary);
                mappingsOut.write(reinterpret_cast<const char*>(inputMappingsBytes.data()), (std::streamsize)inputMappingsBytes.size());
            }

            fs::path bootstrapPath = fs::path(options.outputDir) / (foundJs.stem().string() + ".starproject");
            std::ofstream out(bootstrapPath);
            out << bootConfig.ToText();
        }

        result.success = true;
        result.outputExecutable = outputHtml.string();
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

    fs::path inputMappingsSource = fs::path(options.projectRoot) / "Config" / "InputMappings.cfg";
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
    bootConfig.defaultMap = RewriteToContentRelative(settings.gameDefaultMapPath, contentRoot);
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
            fs::path outputMappingsPath = fs::path(options.outputDir) / "Config" / "InputMappings.cfg";
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
