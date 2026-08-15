#include "BuildRunner.h"
#include "Module.h"
#include "EngineRoot.h"
#include "ExecutablePath.h"
#include "CMakeBuild.h"
#include "BuildDiagnostics.h"
#include "SafeProcess.h"
#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

BuildRunner& BuildRunner::Get() {
    static BuildRunner instance;
    return instance;
}

std::string BuildRunner::GetDefaultBuildDirectory() {
    return EngineRoot::BuildDirectory();
}

std::string BuildRunner::GetDefaultBuildConfig() {
#ifdef STARENGINE_BUILD_CONFIG
    return STARENGINE_BUILD_CONFIG;
#else
    return "Release";
#endif
}

static fs::path LocateBuiltEditorExecutable(const fs::path& buildDir, const std::string& config) {
    std::string exeName = "StarEngine";
#ifdef _WIN32
    exeName += ".exe";
#endif

    fs::path direct = buildDir / exeName;
    if (fs::exists(direct)) return direct;

    if (!config.empty()) {
        fs::path nested = buildDir / config / exeName;
        if (fs::exists(nested)) return nested;
    }
    for (const char* candidate : {"Release", "Debug", "RelWithDebInfo"}) {
        fs::path nested = buildDir / candidate / exeName;
        if (fs::exists(nested)) return nested;
    }
    return {};
}

// Renaming a locked exe aside works on Windows too, unlike overwriting it directly
static void MoveAsideForReplacement(const fs::path& launcherPath) {
    std::error_code ec;
    fs::path oldPath = launcherPath;
    oldPath += ".old";
    fs::remove(oldPath, ec);
    fs::rename(launcherPath, oldPath, ec);
}

BuildResult BuildRunner::RunBuild(const std::string& buildDir, const std::string& config,
                                   const std::function<void(const std::string&)>& onOutput) {
    BuildResult result;

    std::string cmakeMissing = BuildDiagnostics::CheckCMakeAvailable();
    if (!cmakeMissing.empty()) {
        result.success = false;
        result.error = cmakeMissing;
        return result;
    }

    std::string engineRoot = EngineRoot::Resolve();

    if (CMakeBuild::CacheSourceMismatch(buildDir, engineRoot)) {
        std::error_code wipeEc;
        fs::remove_all(buildDir, wipeEc);
    }

    bool hasExistingCache = fs::exists(fs::path(buildDir) / "CMakeCache.txt");

    std::vector<std::string> configureArgs = hasExistingCache
        ? std::vector<std::string>{"cmake", buildDir}
        : std::vector<std::string>{"cmake", "-S", engineRoot, "-B", buildDir, "-DCMAKE_BUILD_TYPE=Release"};

    std::string configureOutput;
    int configureRc = SafeProcess::Run(configureArgs, configureOutput, onOutput);
    result.output += configureOutput;
    if (configureRc != 0) {
        result.success = false;
        result.error = "CMake configure failed";
        std::string hint = BuildDiagnostics::DiagnoseConfigureFailure(configureOutput);
        if (!hint.empty()) result.error += ". " + hint;
        return result;
    }

    std::vector<std::string> buildArgs = {"cmake", "--build", buildDir};
    if (!config.empty()) { buildArgs.push_back("--config"); buildArgs.push_back(config); }

    std::string buildOutput;
    int buildRc = SafeProcess::Run(buildArgs, buildOutput, onOutput);
    result.output += buildOutput;
    if (buildRc != 0) {
        result.success = false;
        result.error = "Build failed";
        return result;
    }

    fs::path builtExe = LocateBuiltEditorExecutable(buildDir, config);
    if (builtExe.empty()) {
        result.success = false;
        result.error = "Build succeeded but the rebuilt editor executable was not found";
        return result;
    }

    std::string runningExe = GetExecutablePath();
    if (runningExe.empty()) {
        result.success = false;
        result.error = "Could not locate the running executable to replace";
        return result;
    }

    MoveAsideForReplacement(runningExe);

    std::error_code ec;
    fs::copy_file(builtExe, runningExe, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.success = false;
        result.error = "Rebuilt the editor, but could not replace the running executable: " + ec.message();
        return result;
    }

    result.success = true;
    return result;
}

void BuildRunner::TriggerRelaunch() {
    if (relaunchCallback) {
        relaunchCallback();
    }
}