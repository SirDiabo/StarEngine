#include "Nodes_Variables.h"
#include "NodeRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"

void RegisterVariableNodes() {
    NodeTemplate getVar;
    getVar.type = "Get_Variable";
    getVar.category = "Variables";
    getVar.moduleName = "Stargraph";
    getVar.createPins = [](Node* node) {
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string varName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") { varName = p.defaultValue; break; }
        }
        for (const Variable& v : graph.GetVariables()) {
            if (v.name == varName) {
                for (Pin& p : node.outputs) {
                    if (p.name == "Value") { p.defaultValue = v.value; break; }
                }
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(getVar);

    NodeTemplate setVar;
    setVar.type = "Set_Variable";
    setVar.category = "Variables";
    setVar.moduleName = "Stargraph";
    setVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Value",         PinType::Float,  true,  false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string varName;
        std::string value;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") varName = p.defaultValue;
            if (p.name == "Value")         value   = p.defaultValue;
        }
        for (Variable& v : graph.GetVariables()) {
            if (v.name != varName) continue;
            
            if (v.replication == ReplicationMode::ServerOnly && !NetworkBackendRegistry::Get().HasAuthority()) break;
            v.value = value;
            break;
        }
    };
    NodeRegistry::Get().RegisterNode(setVar);
}