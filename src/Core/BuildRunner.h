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

    BuildResult RunBuild(const std::string& buildDir);

    void SetRelaunchCallback(std::function<void()> callback) { relaunchCallback = callback; }
    void TriggerRelaunch();

private:
    std::function<void()> relaunchCallback;
};