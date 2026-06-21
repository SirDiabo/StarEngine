#include "Core/Application.h"
#include "Core/Module.h"

int main() {
    Application app;
    if (!app.Initialize()) {
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}