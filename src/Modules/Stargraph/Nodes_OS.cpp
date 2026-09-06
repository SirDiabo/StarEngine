#include "Nodes_OS.h"
#include "NodeRegistry.h"
#include "Runtime/World.h"

void RegisterOSNodes() {
    NodeTemplate loadLevel;
    loadLevel.type = "Load_Level";
    loadLevel.category = "OS";
    loadLevel.moduleName = "Stargraph";
    loadLevel.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Level Path", PinType::String, true, false, 0, "", false, "Map"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadLevel.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string levelPath;
        for (const Pin& p : node.inputs) {
            if (p.name == "Level Path") levelPath = p.defaultValue;
        }
        if (!levelPath.empty()) World::Get().RequestLoadLevel(levelPath);
    };
    NodeRegistry::Get().RegisterNode(loadLevel);

    NodeTemplate quitGame;
    quitGame.type = "Quit_Game";
    quitGame.category = "OS";
    quitGame.moduleName = "Stargraph";
    quitGame.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
    };
    quitGame.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        World::Get().RequestQuit();
    };
    NodeRegistry::Get().RegisterNode(quitGame);
}
