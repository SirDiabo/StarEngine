#include "Nodes_Save.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/SaveGame.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Property.h"
#include "Core/SaveFileFormat.h"
#include "Core/SaveSlots.h"
#include "Core/SavePaths.h"
#include <vector>
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

    void SetOutput(Node& node, const std::string& pinName, const std::string& value) {
        for (Pin& p : node.outputs) if (p.name == pinName) { p.defaultValue = value; return; }
    }

    std::string BoolText(bool value) {
        return value ? "true" : "false";
    }

    std::vector<SlotInfo>& SlotListCache() {
        static std::vector<SlotInfo> cache;
        return cache;
    }

    void RegisterSlotNodes() {
        auto makeNode = [](const char* type, std::function<void(Node*)> pins, std::function<void(Node&, Stargraph&, ExecutionContext&)> run) {
            NodeTemplate t;
            t.type = type;
            t.category = "Save";
            t.moduleName = "Stargraph";
            t.createPins = std::move(pins);
            t.execute = std::move(run);
            NodeRegistry::Get().RegisterNode(t);
        };
        auto execIn = [](Node* n) { n->inputs.push_back({"In", PinType::Exec, true, false}); };
        auto execOut = [](Node* n) { n->outputs.push_back({"Out", PinType::Exec, false, true}); };

        makeNode("Set_Save_Slot",
            [=](Node* n) { execIn(n); n->inputs.push_back({"Slot", PinType::String, true, false, 0, ""}); execOut(n); },
            [](Node& node, Stargraph&, ExecutionContext&) { SaveSlots::SetCurrent(InputValue(node, "Slot")); });

        makeNode("Get_Save_Slot",
            [](Node* n) { n->outputs.push_back({"Slot", PinType::String, false, true}); },
            [](Node& node, Stargraph&, ExecutionContext&) { SetOutput(node, "Slot", SavePaths::GetCurrentSlot()); });

        makeNode("Save_Slot_Exists",
            [](Node* n) {
                n->inputs.push_back({"Slot", PinType::String, true, false, 0, ""});
                n->outputs.push_back({"Exists", PinType::Boolean, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) { SetOutput(node, "Exists", BoolText(SaveSlots::Exists(InputValue(node, "Slot")))); });

        makeNode("Delete_Save_Slot",
            [=](Node* n) {
                execIn(n);
                n->inputs.push_back({"Slot", PinType::String, true, false, 0, ""});
                execOut(n);
                n->outputs.push_back({"Deleted", PinType::Boolean, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) { SetOutput(node, "Deleted", BoolText(SaveSlots::Delete(InputValue(node, "Slot")))); });

        makeNode("Copy_Save_Slot",
            [=](Node* n) {
                execIn(n);
                n->inputs.push_back({"From", PinType::String, true, false, 0, ""});
                n->inputs.push_back({"To", PinType::String, true, false, 0, ""});
                execOut(n);
                n->outputs.push_back({"Copied", PinType::Boolean, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) { SetOutput(node, "Copied", BoolText(SaveSlots::Copy(InputValue(node, "From"), InputValue(node, "To")))); });

        makeNode("Get_Save_Slot_Count",
            [](Node* n) { n->outputs.push_back({"Count", PinType::Integer, false, true}); },
            [](Node& node, Stargraph&, ExecutionContext&) {
                SlotListCache() = SaveSlots::List();
                SetOutput(node, "Count", std::to_string(SlotListCache().size()));
            });

        makeNode("Get_Save_Slot_Name",
            [](Node* n) {
                n->inputs.push_back({"Index", PinType::Integer, true, false, 0, "0"});
                n->outputs.push_back({"Name", PinType::String, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) {
                if (SlotListCache().empty()) SlotListCache() = SaveSlots::List();
                int index = atoi(InputValue(node, "Index").c_str());
                const auto& cache = SlotListCache();
                SetOutput(node, "Name", (index >= 0 && index < (int)cache.size()) ? cache[index].name : "");
            });

        makeNode("Get_Save_Slot_Info",
            [](Node* n) {
                n->inputs.push_back({"Slot", PinType::String, true, false, 0, ""});
                n->outputs.push_back({"Exists", PinType::Boolean, false, true});
                n->outputs.push_back({"Level", PinType::String, false, true});
                n->outputs.push_back({"Timestamp", PinType::String, false, true});
                n->outputs.push_back({"Modified Unix", PinType::Integer, false, true});
                n->outputs.push_back({"Playtime Seconds", PinType::Float, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) {
                SlotInfo info = SaveSlots::GetInfo(InputValue(node, "Slot"));
                SetOutput(node, "Exists", BoolText(info.exists));
                SetOutput(node, "Level", info.levelName);
                SetOutput(node, "Timestamp", SaveSlots::FormatTimestamp(info.modifiedUnix));
                SetOutput(node, "Modified Unix", std::to_string(info.modifiedUnix));
                char buf[48];
                snprintf(buf, sizeof(buf), "%.3f", info.playtimeSeconds);
                SetOutput(node, "Playtime Seconds", buf);
            });

        makeNode("Set_Slot_Metadata",
            [=](Node* n) {
                execIn(n);
                n->inputs.push_back({"Key", PinType::String, true, false, 0, ""});
                n->inputs.push_back({"Value", PinType::String, true, false, 0, ""});
                execOut(n);
            },
            [](Node& node, Stargraph&, ExecutionContext&) { SaveSlots::SetCustomField(InputValue(node, "Key"), InputValue(node, "Value")); });

        makeNode("Get_Slot_Metadata",
            [](Node* n) {
                n->inputs.push_back({"Slot", PinType::String, true, false, 0, ""});
                n->inputs.push_back({"Key", PinType::String, true, false, 0, ""});
                n->outputs.push_back({"Value", PinType::String, false, true});
            },
            [](Node& node, Stargraph&, ExecutionContext&) {
                SetOutput(node, "Value", SaveSlots::GetCustomField(InputValue(node, "Slot"), InputValue(node, "Key")));
            });
    }
}

void RegisterSaveNodes() {
    RegisterSlotNodes();

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
