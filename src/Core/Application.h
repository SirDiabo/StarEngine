#pragma once
#include "Module.h"
#include "Project.h"

class Editor;

class Application {
public:
    bool Initialize(int argc, char* argv[]);
    void Run();
    void Shutdown();

private:
    bool running = false;
    Editor* editor = nullptr;
};