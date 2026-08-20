#include "Nodes_Variables.h"
#include "NodeRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Runtime/Actor.h"
#include "StargraphBehavior.h"
#include "ObjectPinRef.h"

static Stargraph* ResolveVariableGraph(Node& node, Stargraph& graph, ExecutionContext& ctx) {
    for (const Pin& p : node.inputs) {
        if (p.name == "Target" && !p.defaultValue.empty()) {
            Actor* target = ObjectPinRef::ResolveAs<Actor>(node, p.defaultValue, "Actor", ctx);
            if (!target) return nullptr;
            if (target == ctx.actor) return &graph;
            auto* behavior = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior());
            return behavior ? &behavior->GetGraph() : nullptr;
        }
    }
    return &graph;
}

void RegisterVariableNodes() {
    NodeTemplate getVar;
    getVar.type = "Get_Variable";
    getVar.category = "Variables";
    getVar.moduleName = "Stargraph";
    getVar.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string varName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") { varName = p.defaultValue; break; }
        }
        Stargraph* targetGraph = ResolveVariableGraph(node, graph, ctx);
        if (!targetGraph) return;
        if (Variable* v = targetGraph->FindVariable(varName)) {
            for (Pin& p : node.outputs) {
                if (p.name == "Value") { p.defaultValue = v->value; break; }
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
        node->inputs.push_back({"Target",        PinType::Object, true,  false, 0, "", false, "Actor"});
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
        Stargraph* targetGraph = ResolveVariableGraph(node, graph, ctx);
        if (!targetGraph) return;
        if (Variable* v = targetGraph->FindVariable(varName)) {
            if (v->replication == ReplicationMode::ServerOnly && !NetworkBackendRegistry::Get().HasAuthority()) return;
            v->value = value;
        }
    };
    NodeRegistry::Get().RegisterNode(setVar);
}