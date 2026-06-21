#include "GraphExecutor.h"
#include "Stargraph.h"
#include "NodeRegistry.h"

void GraphExecutor::Run(Stargraph& graph, const std::string& eventType) {
    Node* current = graph.FindNodeByType(eventType);
    if (!current) return;

    while (true) {
        Node* next = FollowExecLink(graph, current);
        if (!next) break;

        const NodeTemplate* nodeTemplate = NodeRegistry::Get().GetTemplate(next->type);
        if (nodeTemplate && nodeTemplate->execute) {
            nodeTemplate->execute(*next);
        }

        current = next;
    }
}

Node* GraphExecutor::FollowExecLink(Stargraph& graph, Node* fromNode) {
    for (const Pin& pin : fromNode->outputs) {
        if (pin.type != PinType::Exec) continue;

        for (const Link& link : graph.GetLinks()) {
            if (link.outputPinId == pin.id) {
                return graph.FindNodeByPinId(link.inputPinId);
            }
        }
    }
    return nullptr;
}
