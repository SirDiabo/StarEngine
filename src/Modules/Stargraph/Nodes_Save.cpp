#include "Nodes_Save.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/SaveGame.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Property.h"
#include "Core/SaveFileFormat.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace {
    std::string VariableKey(Actor* actor, const std::string& varName) {
        return actor->GetName() + "." + varName;
    }

    std::string InputValue(const Node& node, const std::string& pinName) {
        for (const Pin& p : node.inputs) if (p.name == pinName) return p.defaultValue;
        return "";
    }
}

void RegisterSaveNodes() {
    NodeTemplate saveVar;
    saveVar.type = "Save_Variable";
    saveVar.category = "Save";
    saveVar.moduleName = "Stargraph";
    saveVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true,  false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        if (!var) return;
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : var->saveFile;
        if (file.empty()) return;
        SaveFileFormat::WriteEntries(file, {{VariableKey(ctx.actor, varName), var->value}});
    };
    NodeRegistry::Get().RegisterNode(saveVar);

    NodeTemplate saveAll;
    saveAll.type = "Save_All";
    saveAll.category = "Save";
    saveAll.moduleName = "Stargraph";
    saveAll.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveAll.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        SaveGame::SaveActor(ctx.actor);
    };
    NodeRegistry::Get().RegisterNode(saveAll);

    NodeTemplate getSaved;
    getSaved.type = "Get_Saved_Variable";
    getSaved.category = "Save";
    getSaved.moduleName = "Stargraph";
    getSaved.createPins = [](Node* node) {
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getSaved.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : (var ? var->saveFile : "");
        if (file.empty()) return;

        auto entries = SaveFileFormat::ReadEntries(file);
        auto it = entries.find(VariableKey(ctx.actor, varName));
        if (it == entries.end()) return;
        for (Pin& p : node.outputs) if (p.name == "Value") { p.defaultValue = it->second; break; }
    };
    NodeRegistry::Get().RegisterNode(getSaved);

    NodeTemplate loadVar;
    loadVar.type = "Load_Saved_Variable";
    loadVar.category = "Save";
    loadVar.moduleName = "Stargraph";
    loadVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true,  false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    loadVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        if (!var) return;
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : var->saveFile;
        if (file.empty()) return;

        auto entries = SaveFileFormat::ReadEntries(file);
        auto it = entries.find(VariableKey(ctx.actor, varName));
        if (it == entries.end()) return;
        var->value = it->second;
        for (Pin& p : node.outputs) if (p.name == "Value") { p.defaultValue = it->second; break; }
    };
    NodeRegistry::Get().RegisterNode(loadVar);

    NodeTemplate loadFile;
    loadFile.type = "Load_File";
    loadFile.category = "Save";
    loadFile.moduleName = "Stargraph";
    loadFile.createPins = [](Node* node) {
        node->inputs.push_back({"In",          PinType::Exec,   true, false});
        node->inputs.push_back({"Target File", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadFile.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string targetFile = InputValue(node, "Target File");
        if (targetFile.empty()) return;
        SaveGame::LoadActor(ctx.actor, targetFile);
    };
    NodeRegistry::Get().RegisterNode(loadFile);

    NodeTemplate loadAll;
    loadAll.type = "Load_All";
    loadAll.category = "Save";
    loadAll.moduleName = "Stargraph";
    loadAll.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadAll.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        SaveGame::LoadActor(ctx.actor);
    };
    NodeRegistry::Get().RegisterNode(loadAll);

    NodeTemplate saveGame;
    saveGame.type = "Save_Game";
    saveGame.category = "Save";
    saveGame.moduleName = "Stargraph";
    saveGame.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveGame.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        SaveGame::SaveWorld();
    };
    NodeRegistry::Get().RegisterNode(saveGame);

    NodeTemplate loadGame;
    loadGame.type = "Load_Game";
    loadGame.category = "Save";
    loadGame.moduleName = "Stargraph";
    loadGame.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadGame.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        SaveGame::LoadWorld();
    };
    NodeRegistry::Get().RegisterNode(loadGame);
}
