#pragma once
#include <string>
#include <functional>

struct BuildResult {
    bool success = false;
    std::string output;
    std::string error;
};

class BuildRunner {
public:
    static BuildRunner& Get();

    BuildResult RunBuild(const std::string& buildDir, const std::string& config = "",
                          const std::function<void(const std::string&)>& onOutput = nullptr);

    static std::string GetDefaultBuildDirectory();

    static std::string GetDefaultBuildConfig();

    void SetRelaunchCallback(std::function<void()> callback) { relaunchCallback = callback; }
    void TriggerRelaunch();

private:
    std::function<void()> relaunchCallback;
};