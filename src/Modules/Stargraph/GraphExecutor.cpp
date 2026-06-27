#include "GraphExecutor.h"
#include "Stargraph.h"
#include "NodeRegistry.h"
#include <cstdlib>

static void PropagateDataPin(Stargraph& graph, Node* toNode, Pin& inputPin, ExecutionContext& ctx) {
    for (const Link& link : graph.GetLinks()) {
        if (link.inputPinId != inputPin.id) continue;

        Node* fromNode = graph.FindNodeByPinId(link.outputPinId);
        if (!fromNode) continue;

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
    if (!current) return;

    if (eventType == "Event_Tick") {
        for (Pin& p : current->outputs) {
            if (p.name == "Delta Seconds") {
                char buf[32];
                snprintf(buf, sizeof(buf), "%f", ctx.deltaTime);
                p.defaultValue = buf;
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