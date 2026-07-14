#include "BuildRunner.h"
#include "Module.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

BuildRunner& BuildRunner::Get() {
    static BuildRunner instance;
    return instance;
}

std::string BuildRunner::GetDefaultBuildDirectory() {
#ifdef STARENGINE_BUILD_DIR
    return STARENGINE_BUILD_DIR;
#else
    return ".";
#endif
}

std::string BuildRunner::GetDefaultBuildConfig() {
#ifdef STARENGINE_BUILD_CONFIG
    return STARENGINE_BUILD_CONFIG;
#else
    return "";
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

static void PrepareRunningExecutableForRebuild() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    std::wstring exePath(path);
    std::wstring oldPath = exePath + L".old";

    DeleteFileW(oldPath.c_str());
    MoveFileExW(exePath.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING);
#endif
}

BuildResult BuildRunner::RunBuild(const std::string& buildDir, const std::string& config) {
    BuildResult result;

    std::string reconfigureOutput;
    bool reconfigureOk = RunShellCommand("cmake \"" + buildDir + "\"", reconfigureOutput);
    result.output += reconfigureOutput;
    if (!reconfigureOk) {
        result.success = false;
        result.error = "CMake reconfigure failed";
        return result;
    }

    PrepareRunningExecutableForRebuild();

    std::string cmd = "cmake --build \"" + buildDir + "\"";
    if (!config.empty()) cmd += " --config \"" + config + "\"";

    std::string buildOutput;
    bool buildOk = RunShellCommand(cmd, buildOutput);
    result.output += buildOutput;
    result.success = buildOk;
    if (!result.success) {
        result.error = "Build failed";
    }

    return result;
}

void BuildRunner::TriggerRelaunch() {
    if (relaunchCallback) {
        relaunchCallback();
    }
}