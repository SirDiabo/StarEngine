#pragma once
#include "Module.h"
#include "Project.h"
#include <string>

class Editor;

class Application {
public:
    bool Initialize(int argc, char* argv[]);
    void Run();
    void Shutdown();

    void SetProjectPath(const std::string& path) { projectPath = path; }
    const std::string& GetProjectPath() const { return projectPath; }

    void RequestRelaunch();

private:
    void Relaunch();

    bool running = false;
    bool pendingRelaunch = false;
    Editor* editor = nullptr;
    std::string projectPath;
};