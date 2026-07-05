#include "GraphExecutor.h"
#include "Stargraph.h"
#include "NodeRegistry.h"
#include <cstdlib>
#include <vector>

static void ResolveWildcardPins(Stargraph& graph, Node* node) {
    bool hasWildcard = false;
    PinType resolvedType = PinType::None;
    
    for (const Pin& pin : node->inputs) {
        if (pin.type == PinType::Wildcard) {
            hasWildcard = true;
            for (const Link& link : graph.GetLinks()) {
                if (link.inputPinId == pin.id) {
                    const Pin* outPin = nullptr;
                    for (const Node& n : graph.GetNodes()) {
                        for (const Pin& p : n.outputs) {
                            if (p.id == link.outputPinId && p.type != PinType::Wildcard) {
                                resolvedType = p.type;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    
    if (!hasWildcard) {
        for (const Pin& pin : node->outputs) {
            if (pin.type == PinType::Wildcard) {
                hasWildcard = true;
                for (const Link& link : graph.GetLinks()) {
                    if (link.outputPinId == pin.id) {
                        const Pin* inPin = nullptr;
                        for (const Node& n : graph.GetNodes()) {
                            for (const Pin& p : n.inputs) {
                                if (p.id == link.inputPinId && p.type != PinType::Wildcard) {
                                    resolvedType = p.type;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    
    if (resolvedType == PinType::None) resolvedType = PinType::Float;
    
    for (Pin& pin : node->inputs) {
        if (pin.type == PinType::Wildcard) pin.type = resolvedType;
    }
    for (Pin& pin : node->outputs) {
        if (pin.type == PinType::Wildcard) pin.type = resolvedType;
    }
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

void GraphExecutor::Run(Stargraph& graph, const std::string& eventType, ExecutionContext ctx) {
    Node* current = graph.FindNodeByType(eventType);
    if (!current) {
        if (eventType == "Event_Tick") {
            std::deque<Node>& nodes = const_cast<std::deque<Node>&>(graph.GetNodes());
            for (Node& node : nodes) {
                const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
                if (!tmpl) continue;
                if (node.type == "Input_Key") {
                    tmpl->execute(node, graph, ctx);
                    bool keyHeld = false;
                    for (const Pin& p : node.outputs) {
                        if (p.type == PinType::Boolean && p.name == "Is Held") {
                            keyHeld = (p.defaultValue == "true");
                            break;
                        }
                    }
                    if (keyHeld) {
                        for (const Pin& outPin : node.outputs) {
                            if (outPin.type == PinType::Exec && outPin.name == "Out") {
                                for (const Link& link : graph.GetLinks()) {
                                    if (link.outputPinId == outPin.id) {
                                        Node* nextNode = graph.FindNodeByPinId(link.inputPinId);
                                        if (nextNode) {
                                            Node* inputCurrent = nextNode;
                                            int inputLimit = 1000;
                                            while (inputLimit-- > 0) {
                                                PropagateDataPins(graph, inputCurrent, ctx);
                                                const NodeTemplate* inputTmpl = NodeRegistry::Get().GetTemplate(inputCurrent->type);
                                                if (inputTmpl && inputTmpl->execute) inputTmpl->execute(*inputCurrent, graph, ctx);
                                                Node* inputNext = FollowExecLink(graph, inputCurrent);
                                                if (!inputNext) break;
                                                inputCurrent = inputNext;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else if (node.type == "Get_Axis_Horizontal" || node.type == "Get_Axis_Vertical") {
                    tmpl->execute(node, graph, ctx);
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
                tmpl->execute(node, graph, ctx);
                bool keyHeld = false;
                for (const Pin& p : node.outputs) {
                    if (p.type == PinType::Boolean && p.name == "Is Held") {
                        keyHeld = (p.defaultValue == "true");
                        break;
                    }
                }
                if (keyHeld) {
                    for (const Pin& outPin : node.outputs) {
                        if (outPin.type == PinType::Exec && outPin.name == "Out") {
                            for (const Link& link : graph.GetLinks()) {
                                if (link.outputPinId == outPin.id) {
                                    Node* nextNode = graph.FindNodeByPinId(link.inputPinId);
                                    if (nextNode) {
                                        Node* inputCurrent = nextNode;
                                        int inputLimit = 1000;
                                        while (inputLimit-- > 0) {
                                            PropagateDataPins(graph, inputCurrent, ctx);
                                            const NodeTemplate* inputTmpl = NodeRegistry::Get().GetTemplate(inputCurrent->type);
                                            if (inputTmpl && inputTmpl->execute) inputTmpl->execute(*inputCurrent, graph, ctx);
                                            Node* inputNext = FollowExecLink(graph, inputCurrent);
                                            if (!inputNext) break;
                                            inputCurrent = inputNext;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (node.type == "Get_Axis_Horizontal" || node.type == "Get_Axis_Vertical") {
                tmpl->execute(node, graph, ctx);
            }
        }
    }

    int safetyLimit = 10000;
    while (safetyLimit-- > 0) {
        Node* next = FollowExecLink(graph, current);
        if (!next) break;

        PropagateDataPins(graph, next, ctx);

        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(next->type);

        if (next->type == "Branch") {
            bool condition = false;
            for (const Pin& p : next->inputs) {
                if (p.name == "Condition") { condition = (p.defaultValue == "true" || p.defaultValue == "1"); break; }
            }
            current = FollowExecLink(graph, next, condition ? "True" : "False");
            if (!current) break;
            continue;
        }

        if (next->type == "For_Loop") {
            PropagateDataPins(graph, next, ctx);
            int first = 0, last = 9;
            for (const Pin& p : next->inputs) {
                if (p.name == "First" && !p.defaultValue.empty()) first = atoi(p.defaultValue.c_str());
                if (p.name == "Last"  && !p.defaultValue.empty()) last  = atoi(p.defaultValue.c_str());
            }
            for (int i = first; i <= last; i++) {
                for (Pin& p : next->outputs) {
                    if (p.name == "Index") p.defaultValue = std::to_string(i);
                }
                Node* body = FollowExecLink(graph, next, "Loop Body");
                if (body) {
                    Node* subCurrent = body;
                    int subLimit = 1000;
                    while (subLimit-- > 0) {
                        PropagateDataPins(graph, subCurrent, ctx);
                        const NodeTemplate* subTmpl = NodeRegistry::Get().GetTemplate(subCurrent->type);
                        if (subTmpl && subTmpl->execute) subTmpl->execute(*subCurrent, graph, ctx);
                        Node* subNext = FollowExecLink(graph, subCurrent);
                        if (!subNext || subNext == next) break;
                        subCurrent = subNext;
                    }
                }
            }
            current = FollowExecLink(graph, next, "Completed");
            if (!current) break;
            continue;
        }

        if (next->type == "Sequence") {
            for (const Pin& outPin : next->outputs) {
                if (outPin.type != PinType::Exec) continue;
                Node* seqNode = FollowExecLink(graph, next, outPin.name);
                if (!seqNode) continue;
                Node* seqCurrent = seqNode;
                int seqLimit = 1000;
                while (seqLimit-- > 0) {
                    PropagateDataPins(graph, seqCurrent, ctx);
                    const NodeTemplate* seqTmpl = NodeRegistry::Get().GetTemplate(seqCurrent->type);
                    if (seqTmpl && seqTmpl->execute) seqTmpl->execute(*seqCurrent, graph, ctx);
                    Node* seqNext = FollowExecLink(graph, seqCurrent);
                    if (!seqNext) break;
                    seqCurrent = seqNext;
                }
            }
            break;
        }

        if (next->type == "While_Loop") {
            while (true) {
                PropagateDataPins(graph, next, ctx);
                bool condition = false;
                for (const Pin& p : next->inputs) {
                    if (p.name == "Condition") { condition = (p.defaultValue == "true" || p.defaultValue == "1"); break; }
                }
                if (!condition) break;
                Node* body = FollowExecLink(graph, next, "Loop Body");
                if (body) {
                    Node* subCurrent = body;
                    int subLimit = 1000;
                    while (subLimit-- > 0) {
                        PropagateDataPins(graph, subCurrent, ctx);
                        const NodeTemplate* subTmpl = NodeRegistry::Get().GetTemplate(subCurrent->type);
                        if (subTmpl && subTmpl->execute) subTmpl->execute(*subCurrent, graph, ctx);
                        Node* subNext = FollowExecLink(graph, subCurrent);
                        if (!subNext || subNext == next) break;
                        subCurrent = subNext;
                    }
                }
            }
            current = FollowExecLink(graph, next, "Completed");
            if (!current) break;
            continue;
        }

        if (tmpl && tmpl->execute) tmpl->execute(*next, graph, ctx);
        current = next;
    }
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