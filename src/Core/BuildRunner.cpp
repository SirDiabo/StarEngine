#include "BuildRunner.h"
#include "Module.h"
#include <cstdio>

BuildRunner& BuildRunner::Get() {
    static BuildRunner instance;
    return instance;
}

BuildResult BuildRunner::RunBuild(const std::string& buildDir) {
    BuildResult result;
    std::string cmd = "cmake --build \"" + buildDir + "\" 2>&1";
    
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(cmd.c_str(), "r");
#else
    pipe = popen(cmd.c_str(), "r");
#endif
    
    if (!pipe) {
        result.success = false;
        result.error = "Failed to start build process";
        return result;
    }
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result.output += buffer;
    }
    
#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif
    
    result.success = (exitCode == 0);
    if (!result.success) {
        result.error = "Build failed with exit code " + std::to_string(exitCode);
    }
    
    return result;
}

void BuildRunner::TriggerRelaunch() {
    if (relaunchCallback) {
        relaunchCallback();
    }
}