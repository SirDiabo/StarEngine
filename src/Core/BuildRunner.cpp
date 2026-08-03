#include "BuildRunner.h"
#include "Module.h"
#include "EngineRoot.h"
#include "ExecutablePath.h"
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

static bool RunShellCommand(const std::string& cmd, std::string& outOutput) {
    std::string fullCmd = cmd + " 2>&1";
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(fullCmd.c_str(), "r");
#else
    pipe = popen(fullCmd.c_str(), "r");
#endif
    if (!pipe) return false;

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        outOutput += buffer;
    }

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif
    return exitCode == 0;
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

static void MoveAsideForReplacement(const fs::path& launcherPath) {
    std::error_code ec;
    fs::path oldPath = launcherPath;
    oldPath += ".old";
    fs::remove(oldPath, ec);
    fs::rename(launcherPath, oldPath, ec);
}

BuildResult BuildRunner::RunBuild(const std::string& buildDir, const std::string& config) {
    BuildResult result;

    std::string engineRoot = EngineRoot::Resolve();
    bool hasExistingCache = fs::exists(fs::path(buildDir) / "CMakeCache.txt");

    std::string configureOutput;
    std::string configureCmd = hasExistingCache
        ? "cmake \"" + buildDir + "\""
        : "cmake -S \"" + engineRoot + "\" -B \"" + buildDir + "\" -DCMAKE_BUILD_TYPE=Release";
    bool configureOk = RunShellCommand(configureCmd, configureOutput);
    result.output += configureOutput;
    if (!configureOk) {
        result.success = false;
        result.error = "CMake configure failed";
        return result;
    }

    std::string cmd = "cmake --build \"" + buildDir + "\"";
    if (!config.empty()) cmd += " --config \"" + config + "\"";

    std::string buildOutput;
    bool buildOk = RunShellCommand(cmd, buildOutput);
    result.output += buildOutput;
    if (!buildOk) {
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