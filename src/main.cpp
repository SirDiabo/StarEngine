#include "Core/Application.h"
#include "Core/Module.h"
#include "Modules/Stargraph/StargraphModule.h"
#include "Modules/Stargraph/StargraphBehavior.h"
#include "Runtime/Actor.h"
#include <utility>

static void RunStargraphDemo() {
    Stargraph graph;
    Node* beginPlay = graph.CreateNode("Event_BeginPlay");
    Node* printString = graph.CreateNode("Print_String");

    printString->inputs[1].defaultValue = "Hello from StarEngine";

    Link link;
    link.outputPinId = beginPlay->outputs[0].id;
    link.inputPinId = printString->inputs[0].id;
    graph.AddLink(link);

    Actor actor;
    actor.SetName("DemoActor");
    actor.SetGraphBehavior(new StargraphBehavior(std::move(graph)));
    actor.BeginPlay();
}

int main() {
    ModuleManager::Get().Register(new StargraphModule());

    Application app;
    if (!app.Initialize()) {
        return 1;
    }

    RunStargraphDemo();

    app.Run();
    app.Shutdown();
    return 0;
}
