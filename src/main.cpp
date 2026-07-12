#include "Core/Application.h"
#include "Core/Module.h"
#include "Modules.modules.generated.h"

#ifdef STARENGINE_DEBUG_CONSOLE
#include <windows.h>
#include <cstdio>
#endif

int main(int argc, char* argv[]) {
#ifdef STARENGINE_DEBUG_CONSOLE
    AllocConsole();
    FILE* consoleOut = nullptr;
    FILE* consoleErr = nullptr;
    freopen_s(&consoleOut, "CONOUT$", "w", stdout);
    freopen_s(&consoleErr, "CONOUT$", "w", stderr);
#endif

    RegisterAllModules();

    Application app;
    if (!app.Initialize(argc, argv)) {
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
