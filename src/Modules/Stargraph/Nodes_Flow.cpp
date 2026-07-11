#include "Nodes_Flow.h"
#include "NodeRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"
#include <unordered_map>

static std::unordered_map<std::string, bool> doOnceStates;
static std::unordered_map<std::string, bool> gateStates;
static std::unordered_map<std::string, bool> flipFlopStates;

void RegisterFlowNodes() {
    NodeTemplate branch;
    branch.type = "Branch";
    branch.category = "Flow";
    branch.moduleName = "Stargraph";
    branch.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"True",  PinType::Exec, false, true});
        node->outputs.push_back({"False", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(branch);

    NodeTemplate hasAuthority;
    hasAuthority.type = "Has_Authority";
    hasAuthority.category = "Flow";
    hasAuthority.moduleName = "Stargraph";
    hasAuthority.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Authority", PinType::Exec, false, true});
        node->outputs.push_back({"Remote",    PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(hasAuthority);

    NodeTemplate forLoop;
    forLoop.type = "For_Loop";
    forLoop.category = "Flow";
    forLoop.moduleName = "Stargraph";
    forLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec,    true,  false});
        node->inputs.push_back({"First", PinType::Integer, true,  false, 0, "0"});
        node->inputs.push_back({"Last",  PinType::Integer, true,  false, 0, "9"});
        node->outputs.push_back({"Loop Body", PinType::Exec,    false, true});
        node->outputs.push_back({"Index",     PinType::Integer, false, true});
        node->outputs.push_back({"Completed", PinType::Exec,    false, true});
    };
    NodeRegistry::Get().RegisterNode(forLoop);

    NodeTemplate whileLoop;
    whileLoop.type = "While_Loop";
    whileLoop.category = "Flow";
    whileLoop.moduleName = "Stargraph";
    whileLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"Loop Body", PinType::Exec, false, true});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(whileLoop);

    NodeTemplate sequence;
    sequence.type = "Sequence";
    sequence.category = "Flow";
    sequence.moduleName = "Stargraph";
    sequence.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"0", PinType::Exec, false, true});
        node->outputs.push_back({"1", PinType::Exec, false, true});
        node->outputs.push_back({"2", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(sequence);

    NodeTemplate doOnce;
    doOnce.type = "Do_Once";
    doOnce.category = "Flow";
    doOnce.moduleName = "Stargraph";
    doOnce.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Reset", PinType::Exec, false, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    doOnce.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string nodeId = node.id;
        bool& state = doOnceStates[nodeId];

        for (const Pin& p : node.inputs) {
            if (p.name == "Reset") {
                state = false;
            }
        }

        if (!state) {
            for (Pin& p : node.outputs) if (p.name == "Out") {
                for (const Link& lk : graph.GetLinks()) {
                    if (lk.outputPinId == p.id) {
                        Node* next = graph.FindNodeByPinId(lk.inputPinId);
                        if (next) {
                            NodeTemplate* tmpl = const_cast<NodeTemplate*>(NodeRegistry::Get().GetTemplate(next->type));
                            if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
                        }
                    }
                }
            }
            state = true;
        }
    };
    NodeRegistry::Get().RegisterNode(doOnce);

    NodeTemplate gate;
    gate.type = "Gate";
    gate.category = "Flow";
    gate.moduleName = "Stargraph";
    gate.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec, true, false});
        node->inputs.push_back({"Open",      PinType::Exec, false, false});
        node->inputs.push_back({"Close",     PinType::Exec, false, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    gate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string nodeId = node.id;
        bool& state = gateStates[nodeId];

        for (const Pin& p : node.inputs) {
            if (p.name == "Open") state = true;
            if (p.name == "Close") state = false;
        }

        if (state) {
            for (Pin& p : node.outputs) if (p.name == "Out") {
                for (const Link& lk : graph.GetLinks()) {
                    if (lk.outputPinId == p.id) {
                        Node* next = graph.FindNodeByPinId(lk.inputPinId);
                        if (next) {
                            NodeTemplate* tmpl = const_cast<NodeTemplate*>(NodeRegistry::Get().GetTemplate(next->type));
                            if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
                        }
                    }
                }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(gate);

    NodeTemplate flipFlop;
    flipFlop.type = "Flip_Flop";
    flipFlop.category = "Flow";
    flipFlop.moduleName = "Stargraph";
    flipFlop.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"True",  PinType::Exec, false, true});
        node->outputs.push_back({"False", PinType::Exec, false, true});
    };
    flipFlop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string nodeId = node.id;
        bool& state = flipFlopStates[nodeId];
        state = !state;

        const char* outputName = state ? "True" : "False";
        for (Pin& p : node.outputs) if (p.name == outputName) {
            for (const Link& lk : graph.GetLinks()) {
                if (lk.outputPinId == p.id) {
                    Node* next = graph.FindNodeByPinId(lk.inputPinId);
                    if (next) {
                        NodeTemplate* tmpl = const_cast<NodeTemplate*>(NodeRegistry::Get().GetTemplate(next->type));
                        if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
                    }
                }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(flipFlop);
}