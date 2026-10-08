#include "GraphExecutor.h"
#include "Stargraph.h"
#include "NodeRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Runtime/World.h"
#include "Core/Profiler.h"
#include "raylib.h"
#include <cstdlib>
#include <string>
#include <vector>

static std::string NodeProfileLabel(const Node& node) {
    std::string label = node.title.empty() ? node.type : node.title;
    for (char& c : label) if (c == '_') c = ' ';
    return label + " #" + node.id;
}

static std::string ProfileLabel(const char* prefix, const std::string& text) {
    std::string label = text;
    for (char& c : label) if (c == '_') c = ' ';
    return prefix + label;
}

static bool NodeHasExecPin(const Node& node) {
    for (const Pin& pin : node.inputs)  if (pin.type == PinType::Exec) return true;
    for (const Pin& pin : node.outputs) if (pin.type == PinType::Exec) return true;
    return false;
}

static void ResolveWildcardPins(Stargraph& graph, Node* node) {
    if (node->wildcardResolved) return;

    bool hasWildcard = false;
    for (const Pin& pin : node->inputs)  if (pin.isWildcard) hasWildcard = true;
    for (const Pin& pin : node->outputs) if (pin.isWildcard) hasWildcard = true;
    if (!hasWildcard) { node->wildcardResolved = true; return; }

    PinType resolvedType = PinType::None;

    for (const Pin& pin : node->inputs) {
        if (!pin.isWildcard) continue;
        Link* link = graph.GetLinkToPin(pin.id);
        if (!link) continue;
        Node* fromNode = graph.FindNodeByPinId(link->outputPinId);
        if (!fromNode) continue;
        for (const Pin& p : fromNode->outputs) {
            if (p.id == link->outputPinId && p.type != PinType::Wildcard) {
                resolvedType = p.type;
            }
        }
    }

    if (resolvedType == PinType::None) {
        for (const Pin& pin : node->outputs) {
            if (!pin.isWildcard) continue;
            for (Link* link : graph.GetLinksFromPin(pin.id)) {
                Node* toNode = graph.FindNodeByPinId(link->inputPinId);
                if (!toNode) continue;
                for (const Pin& p : toNode->inputs) {
                    if (p.id == link->inputPinId && p.type != PinType::Wildcard) {
                        resolvedType = p.type;
                    }
                }
            }
        }
    }

    PinType finalType = (resolvedType != PinType::None) ? resolvedType : PinType::Wildcard;

    for (Pin& pin : node->inputs)  if (pin.isWildcard) pin.type = finalType;
    for (Pin& pin : node->outputs) if (pin.isWildcard) pin.type = finalType;

    if (finalType != PinType::Wildcard) node->wildcardResolved = true;
}

static void PropagateDataPin(Stargraph& graph, Node* toNode, Pin& inputPin, ExecutionContext& ctx, std::vector<int>& visiting) {
    for (int id : visiting) if (id == inputPin.id) return; // cycle: this pin is already being resolved further up the chain

    Link* link = graph.GetLinkToPin(inputPin.id);
    if (!link) return;

    Node* fromNode = graph.FindNodeByPinId(link->outputPinId);
    if (!fromNode) return;

    visiting.push_back(inputPin.id);
    PROFILE_SCOPE_DYN_L(Profiler::DetailNodes, NodeProfileLabel(*fromNode));

    ResolveWildcardPins(graph, fromNode);
    ResolveWildcardPins(graph, toNode);

    for (Pin& upPin : fromNode->inputs) {
        if (upPin.type != PinType::Exec) PropagateDataPin(graph, fromNode, upPin, ctx, visiting);
    }

    bool fromNodeIsImpure = NodeHasExecPin(*fromNode);
    bool alreadyExecuted = false;
    if (fromNodeIsImpure) {
        int fromId = std::stoi(fromNode->id);
        for (int id : ctx.executedNodeIds) if (id == fromId) { alreadyExecuted = true; break; }
    }

    if (!alreadyExecuted) {
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(fromNode->type);
        if (tmpl && tmpl->execute) tmpl->execute(*fromNode, graph, ctx);
        if (fromNodeIsImpure) ctx.executedNodeIds.push_back(std::stoi(fromNode->id));
    }

    for (const Pin& outPin : fromNode->outputs) {
        if (outPin.id == link->outputPinId) {
            if (inputPin.defaultValue != outPin.defaultValue) {
                inputPin.defaultValue = outPin.defaultValue;
            }
            break;
        }
    }

    visiting.pop_back();
}

void GraphExecutor::PropagateDataPins(Stargraph& graph, Node* node, ExecutionContext& ctx) {
    std::vector<int> visiting;
    for (Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec) continue;
        PropagateDataPin(graph, node, pin, ctx, visiting);
    }
}

void GraphExecutor::ExecuteLinkedNodes(Stargraph& graph, Node* startNode, ExecutionContext& ctx, int enteredPinId) {
    Node* current = startNode;
    int enteredPin = enteredPinId;
    int limit = 1000;
    while (current && limit-- > 0) {
        ctx.enteredExecPin = enteredPin;
        PROFILE_SCOPE_DYN_L(Profiler::DetailNodes, NodeProfileLabel(*current));
        GraphExecutor::PropagateDataPins(graph, current, ctx);
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(current->type);
        if (tmpl && tmpl->execute) tmpl->execute(*current, graph, ctx);
        if (NodeHasExecPin(*current)) ctx.executedNodeIds.push_back(std::stoi(current->id));
        Node* next = nullptr;
        int nextPinId = 0;
        for (const Pin& pin : current->outputs) {
            if (pin.type != PinType::Exec) continue;
            if (pin.name != "Out") continue;
            for (Link* link : graph.GetLinksFromPin(pin.id)) {
                next = graph.FindNodeByPinId(link->inputPinId);
                nextPinId = link->inputPinId;
                break;
            }
            if (next) break;
        }
        current = next;
        enteredPin = nextPinId;
    }
}

void GraphExecutor::RunFromPin(Stargraph& graph, Node* node, ExecutionContext& ctx, const std::string& pinName) {
    for (const Pin& pin : node->outputs) {
        if (pin.name != pinName) continue;
        for (Link* link : graph.GetLinksFromPin(pin.id)) {
            ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link->inputPinId), ctx, link->inputPinId);
        }
    }
}

static void ProcessHeldEventNode(Node& node, Stargraph& graph, ExecutionContext& ctx) {
    if (ctx.actor != World::Get().GetInputFocus()) return;

    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->execute) return;

    GraphExecutor::PropagateDataPins(graph, &node, ctx);
    tmpl->execute(node, graph, ctx);

    bool held = false;
    for (const Pin& p : node.outputs) {
        if (p.type == PinType::Boolean && p.name == "Is Held") {
            held = (p.defaultValue == "true");
            break;
        }
    }

    bool wasHeld = node.heldPrevState;
    node.heldPrevState = held;

    if (!held && wasHeld) {
        for (const Pin& outPin : node.outputs) {
            if (outPin.type == PinType::Exec && outPin.name == "Completed") {
                for (Link* link : graph.GetLinksFromPin(outPin.id)) {
                    GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link->inputPinId), ctx, link->inputPinId);
                }
            }
        }
    }

    if (held) {
        if (!wasHeld) {
            for (const Pin& outPin : node.outputs) {
                if (outPin.type == PinType::Exec && outPin.name == "Started") {
                    for (Link* link : graph.GetLinksFromPin(outPin.id)) {
                        GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link->inputPinId), ctx, link->inputPinId);
                    }
                }
            }
        }

        for (const Pin& outPin : node.outputs) {
            if (outPin.type == PinType::Exec && outPin.name == "Triggered") {
                for (Link* link : graph.GetLinksFromPin(outPin.id)) {
                    GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link->inputPinId), ctx, link->inputPinId);
                }
            }
        }
    }
}

void GraphExecutor::Run(Stargraph& graph, const std::string& eventType, ExecutionContext ctx) {
    if (!ctx.rootGraph) ctx.rootGraph = &graph;

    if (eventType == "Event_Tick") {
        PROFILE_SCOPE("Input Event Polling");
        for (Node* node : graph.GetNodesOfType("Input_Key")) {
            ProcessHeldEventNode(*node, graph, ctx);
        }
        for (Node* node : graph.GetNodesOfType("Input_Mapping")) {
            ProcessHeldEventNode(*node, graph, ctx);
        }
    }

    Node* current = graph.FindNodeByType(eventType);
    if (!current) return;
    PROFILE_SCOPE_DYN(ProfileLabel("", eventType));

    if (eventType == "Event_Tick") {
        for (Pin& p : current->outputs) {
            if (p.name == "Delta Seconds") {
                char buf[32];
                snprintf(buf, sizeof(buf), "%f", ctx.deltaTime);
                p.defaultValue = buf;
            }
        }
    }

    ExecuteFromNode(graph, current, ctx);
}

void GraphExecutor::ExecuteFromNode(Stargraph& graph, Node* startNode, ExecutionContext& ctx) {
    Node* current = startNode;
    int safetyLimit = 10000;
    while (safetyLimit-- > 0) {
        int nextPinId = 0;
        Node* next = FollowExecLink(graph, current, "Out", &nextPinId);
        if (!next) break;

        ctx.enteredExecPin = nextPinId;
        PROFILE_SCOPE_DYN_L(Profiler::DetailNodes, NodeProfileLabel(*next));
        PropagateDataPins(graph, next, ctx);

        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(next->type);

        if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
        if (NodeHasExecPin(*next)) ctx.executedNodeIds.push_back(std::stoi(next->id));
        current = next;
    }
}

void GraphExecutor::RunCustomEvent(Stargraph& graph, const std::string& eventName, ExecutionContext ctx,
                                    const std::vector<std::pair<std::string, std::string>>& params) {
    if (!ctx.rootGraph) ctx.rootGraph = &graph;
    if (graph.customEventDepth > 32 || ctx.callDepth > 32) return;

    Node* eventNode = graph.FindCustomEventByName(eventName);
    if (!eventNode) return;
    PROFILE_SCOPE_DYN(ProfileLabel("Custom Event: ", eventName));

    for (const auto& param : params) {
        for (Pin& p : eventNode->outputs) {
            if (p.name == param.first) { p.defaultValue = param.second; break; }
        }
    }

    graph.customEventDepth++;
    ctx.callDepth++;
    ExecuteFromNode(graph, eventNode, ctx);
    ctx.callDepth--;
    graph.customEventDepth--;
}

bool GraphExecutor::RunFunction(Stargraph& classGraph, const std::string& functionName, ExecutionContext ctx,
                                 const std::vector<std::pair<std::string, std::string>>& args,
                                 std::vector<std::pair<std::string, std::string>>& outResults) {
    if (!ctx.rootGraph) ctx.rootGraph = &classGraph;
    if (ctx.callDepth > 32) {
        TraceLog(LOG_WARNING, "GraphExecutor: function call depth exceeded 32 (recursive Call_Function?), stopping at \"%s\"", functionName.c_str());
        return false;
    }

    FunctionDef* fn = classGraph.FindFunction(functionName);
    if (!fn) return false;

    Node* entry = fn->graph.FindNodeByType("Function_Entry");
    if (!entry) return false;
    PROFILE_SCOPE_DYN(ProfileLabel("Function: ", functionName));

    for (Variable& localVar : fn->graph.GetVariables()) {
        localVar.value = localVar.defaultValue;
    }

    for (const auto& arg : args) {
        for (Pin& p : entry->outputs) {
            if (p.name == arg.first) { p.defaultValue = arg.second; break; }
        }
    }

    ctx.callDepth++;
    ExecuteFromNode(fn->graph, entry, ctx);

    Node* ret = fn->graph.FindNodeByType("Return");
    outResults.clear();
    if (ret) {
        GraphExecutor::PropagateDataPins(fn->graph, ret, ctx);
        for (const Pin& p : ret->inputs) {
            if (p.type == PinType::Exec) continue;
            outResults.emplace_back(p.name, (const std::string&)p.defaultValue);
        }
    }
    ctx.callDepth--;

    return true;
}

Node* GraphExecutor::FollowExecLink(Stargraph& graph, Node* fromNode, const std::string& pinName, int* outPinId) {
    for (const Pin& pin : fromNode->outputs) {
        if (pin.type != PinType::Exec) continue;
        if (pin.name != pinName) continue;
        for (Link* link : graph.GetLinksFromPin(pin.id)) {
            if (outPinId) *outPinId = link->inputPinId;
            return graph.FindNodeByPinId(link->inputPinId);
        }
    }
    return nullptr;
}

bool GraphExecutor::EnteredViaPin(const Node& node, const ExecutionContext& ctx, const std::string& pinName) {
    for (const Pin& pin : node.inputs) {
        if (pin.name == pinName) return ctx.enteredExecPin == pin.id;
    }
    return false;
}
