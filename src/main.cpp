#include "Core/Application.h"
#include "Core/Module.h"
#include "Modules/Stargraph/StargraphModule.h"

int main(int argc, char* argv[]) {
    ModuleManager::Get().Register(new StargraphModule());

    Application app;
    if (!app.Initialize(argc, argv)) {
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
