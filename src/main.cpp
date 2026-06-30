#include "Core/Application.h"
#include "Core/Module.h"
#include "Modules.modules.generated.h"

int main(int argc, char* argv[]) {
    RegisterAllModules();

    Application app;
    if (!app.Initialize(argc, argv)) {
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
