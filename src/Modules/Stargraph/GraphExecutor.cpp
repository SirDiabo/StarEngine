#include "GraphExecutor.h"
#include "Stargraph.h"
#include "NodeRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"
#include <cstdlib>
#include <vector>
#include <unordered_map>

static void ResolveWildcardPins(Stargraph& graph, Node* node) {
    bool hasWildcard = false;
    for (const Pin& pin : node->inputs)  if (pin.isWildcard) hasWildcard = true;
    for (const Pin& pin : node->outputs) if (pin.isWildcard) hasWildcard = true;
    if (!hasWildcard) return;

    PinType resolvedType = PinType::None;

    for (const Pin& pin : node->inputs) {
        if (!pin.isWildcard) continue;
        for (const Link& link : graph.GetLinks()) {
            if (link.inputPinId != pin.id) continue;
            for (const Node& n : graph.GetNodes()) {
                for (const Pin& p : n.outputs) {
                    if (p.id == link.outputPinId && p.type != PinType::Wildcard) {
                        resolvedType = p.type;
                    }
                }
            }
        }
    }

    if (resolvedType == PinType::None) {
        for (const Pin& pin : node->outputs) {
            if (!pin.isWildcard) continue;
            for (const Link& link : graph.GetLinks()) {
                if (link.outputPinId != pin.id) continue;
                for (const Node& n : graph.GetNodes()) {
                    for (const Pin& p : n.inputs) {
                        if (p.id == link.inputPinId && p.type != PinType::Wildcard) {
                            resolvedType = p.type;
                        }
                    }
                }
            }
        }
    }

    PinType finalType = (resolvedType != PinType::None) ? resolvedType : PinType::Wildcard;

    for (Pin& pin : node->inputs)  if (pin.isWildcard) pin.type = finalType;
    for (Pin& pin : node->outputs) if (pin.isWildcard) pin.type = finalType;
}

static void PropagateDataPin(Stargraph& graph, Node* toNode, Pin& inputPin, ExecutionContext& ctx) {
    for (const Link& link : graph.GetLinks()) {
        if (link.inputPinId != inputPin.id) continue;

        Node* fromNode = graph.FindNodeByPinId(link.outputPinId);
        if (!fromNode) continue;

        ResolveWildcardPins(graph, fromNode);
        ResolveWildcardPins(graph, toNode);

        for (Pin& upPin : fromNode->inputs) {
            if (upPin.type != PinType::Exec) PropagateDataPin(graph, fromNode, upPin, ctx);
        }

        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(fromNode->type);
        if (tmpl && tmpl->execute) tmpl->execute(*fromNode, graph, ctx);

        for (const Pin& outPin : fromNode->outputs) {
            if (outPin.id == link.outputPinId) {
                inputPin.defaultValue = outPin.defaultValue;
                break;
            }
        }
        break;
    }
}

void GraphExecutor::PropagateDataPins(Stargraph& graph, Node* node, ExecutionContext& ctx) {
    for (Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec) continue;
        PropagateDataPin(graph, node, pin, ctx);
    }
}

void GraphExecutor::ExecuteLinkedNodes(Stargraph& graph, Node* startNode, ExecutionContext& ctx) {
    Node* current = startNode;
    int limit = 1000;
    while (current && limit-- > 0) {
        GraphExecutor::PropagateDataPins(graph, current, ctx);
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(current->type);
        if (tmpl && tmpl->execute) tmpl->execute(*current, graph, ctx);
        Node* next = nullptr;
        for (const Pin& pin : current->outputs) {
            if (pin.type != PinType::Exec) continue;
            if (pin.name != "Out") continue;
            for (const Link& link : graph.GetLinks()) {
                if (link.outputPinId == pin.id) {
                    next = graph.FindNodeByPinId(link.inputPinId);
                    break;
                }
            }
            if (next) break;
        }
        current = next;
    }
}

static void ProcessInputKey(Node& node, Stargraph& graph, ExecutionContext& ctx) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->execute) return;

    tmpl->execute(node, graph, ctx);

    bool held = false;
    for (const Pin& p : node.outputs) {
        if (p.type == PinType::Boolean && p.name == "Is Held") {
            held = (p.defaultValue == "true");
            break;
        }
    }

    static std::unordered_map<std::string, bool> prevHeld;
    bool wasHeld = false;
    auto it = prevHeld.find(node.id);
    if (it != prevHeld.end()) wasHeld = it->second;
    prevHeld[node.id] = held;

    if (!held && wasHeld) {
        for (const Pin& outPin : node.outputs) {
            if (outPin.type == PinType::Exec && outPin.name == "Completed") {
                for (const Link& link : graph.GetLinks()) {
                    if (link.outputPinId == outPin.id) {
                        GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link.inputPinId), ctx);
                    }
                }
            }
        }
    }

    if (held) {
        if (!wasHeld) {
            for (const Pin& outPin : node.outputs) {
                if (outPin.type == PinType::Exec && outPin.name == "Started") {
                    for (const Link& link : graph.GetLinks()) {
                        if (link.outputPinId == outPin.id) {
                            GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link.inputPinId), ctx);
                        }
                    }
                }
            }
        }

        for (const Pin& outPin : node.outputs) {
            if (outPin.type == PinType::Exec && outPin.name == "Triggered") {
                for (const Link& link : graph.GetLinks()) {
                    if (link.outputPinId == outPin.id) {
                        GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(link.inputPinId), ctx);
                    }
                }
            }
        }
    }
}

void GraphExecutor::Run(Stargraph& graph, const std::string& eventType, ExecutionContext ctx) {
    Node* current = graph.FindNodeByType(eventType);
    if (!current) {
        if (eventType == "Event_Tick") {
            std::deque<Node>& nodes = const_cast<std::deque<Node>&>(graph.GetNodes());
            for (Node& node : nodes) {
                const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
                if (!tmpl) continue;
                if (node.type == "Input_Key") {
                    ProcessInputKey(node, graph, ctx);
                }
            }
        }
        return;
    }

    if (eventType == "Event_Tick") {
        for (Pin& p : current->outputs) {
            if (p.name == "Delta Seconds") {
                char buf[32];
                snprintf(buf, sizeof(buf), "%f", ctx.deltaTime);
                p.defaultValue = buf;
            }
        }
        std::deque<Node>& nodes = const_cast<std::deque<Node>&>(graph.GetNodes());
        for (Node& node : nodes) {
            const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
            if (!tmpl) continue;
            if (node.type == "Input_Key") {
                ProcessInputKey(node, graph, ctx);
            }
        }
    }

    ExecuteFromNode(graph, current, ctx);
}

void GraphExecutor::ExecuteFromNode(Stargraph& graph, Node* startNode, ExecutionContext& ctx) {
    Node* current = startNode;
    int safetyLimit = 10000;
    while (safetyLimit-- > 0) {
        Node* next = FollowExecLink(graph, current);
        if (!next) break;

        PropagateDataPins(graph, next, ctx);

        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(next->type);

        if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
        current = next;
    }
}

void GraphExecutor::RunCustomEvent(Stargraph& graph, const std::string& eventName, ExecutionContext ctx,
                                    const std::vector<std::pair<std::string, std::string>>& params) {
    static int depth = 0;
    if (depth > 32) return; // guard against event chains calling each other recursively forever

    Node* eventNode = graph.FindCustomEventByName(eventName);
    if (!eventNode) return;

    for (const auto& param : params) {
        for (Pin& p : eventNode->outputs) {
            if (p.name == param.first) { p.defaultValue = param.second; break; }
        }
    }

    depth++;
    ExecuteFromNode(graph, eventNode, ctx);
    depth--;
}

Node* GraphExecutor::FollowExecLink(Stargraph& graph, Node* fromNode, const std::string& pinName) {
    for (const Pin& pin : fromNode->outputs) {
        if (pin.type != PinType::Exec) continue;
        if (pin.name != pinName) continue;
        for (const Link& link : graph.GetLinks()) {
            if (link.outputPinId == pin.id) {
                return graph.FindNodeByPinId(link.inputPinId);
            }
        }
    }
    return nullptr;
}