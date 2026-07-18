#include "Nodes_Flow.h"
#include "NodeRegistry.h"
#include "GraphExecutor.h"
#include "Runtime/Networking/NetworkBackend.h"

static void RunFromPin(Node& node, Stargraph& graph, ExecutionContext& ctx, const char* pinName) {
    for (const Pin& p : node.outputs) {
        if (p.name != pinName) continue;
        for (const Link& lk : graph.GetLinks()) {
            if (lk.outputPinId == p.id) {
                GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(lk.inputPinId), ctx);
            }
        }
    }
}

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
    branch.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool condition = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Condition") condition = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        RunFromPin(node, graph, ctx, condition ? "True" : "False");
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
    hasAuthority.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool authority = NetworkBackendRegistry::Get().HasAuthority();
        RunFromPin(node, graph, ctx, authority ? "Authority" : "Remote");
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
    forLoop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int first = 0, last = 9;
        for (const Pin& p : node.inputs) {
            if (p.name == "First" && !p.defaultValue.empty()) first = p.defaultValue.AsInt();
            if (p.name == "Last" && !p.defaultValue.empty()) last = p.defaultValue.AsInt();
        }
        for (int i = first; i <= last; i++) {
            char buf[64]; snprintf(buf, sizeof(buf), "%d", i);
            for (Pin& p : node.outputs) if (p.name == "Index") p.defaultValue = buf;
            RunFromPin(node, graph, ctx, "Loop Body");
        }
        RunFromPin(node, graph, ctx, "Completed");
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
    whileLoop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int loopLimit = 1000;
        while (true) {
            GraphExecutor::PropagateDataPins(graph, &node, ctx);
            bool condition = false;
            for (const Pin& p : node.inputs) {
                if (p.name == "Condition") { condition = (p.defaultValue == "true" || p.defaultValue == "1"); break; }
            }
            if (!condition) break;
            RunFromPin(node, graph, ctx, "Loop Body");
            if (--loopLimit <= 0) break;
        }
        RunFromPin(node, graph, ctx, "Completed");
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
    sequence.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (const Pin& outPin : node.outputs) {
            if (outPin.type != PinType::Exec) continue;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.outputPinId == outPin.id) {
                    GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(lk.inputPinId), ctx);
                }
            }
        }
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
        for (const Pin& p : node.inputs) {
            if (p.name == "Reset") {
                node.flowState = false;
            }
        }

        if (!node.flowState) {
            RunFromPin(node, graph, ctx, "Out");
            node.flowState = true;
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
        for (const Pin& p : node.inputs) {
            if (p.name == "Open") node.flowState = true;
            if (p.name == "Close") node.flowState = false;
        }

        if (node.flowState) {
            RunFromPin(node, graph, ctx, "Out");
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
        node.flowState = !node.flowState;

        RunFromPin(node, graph, ctx, node.flowState ? "True" : "False");
    };
    NodeRegistry::Get().RegisterNode(flipFlop);
}