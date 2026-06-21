#include "Stargraph.h"
#include "NodeRegistry.h"

Node* Stargraph::CreateNode(const std::string& type) {
    nodes.emplace_back();
    Node* node = &nodes.back();
    node->id = std::to_string(nodes.size());
    node->type = type;
    node->title = type;

    const NodeTemplate* nodeTemplate = NodeRegistry::Get().GetTemplate(type);
    if (nodeTemplate && nodeTemplate->createPins) {
        nodeTemplate->createPins(node);
    }

    for (Pin& pin : node->inputs) pin.id = nextPinId++;
    for (Pin& pin : node->outputs) pin.id = nextPinId++;

    return node;
}

void Stargraph::AddLink(const Link& link) {
    links.push_back(link);
}

void Stargraph::AddVariable(const Variable& var) {
    variables.push_back(var);
}

Pin* Stargraph::FindPin(int pinId) {
    for (Node& node : nodes) {
        for (Pin& pin : node.inputs) {
            if (pin.id == pinId) return &pin;
        }
        for (Pin& pin : node.outputs) {
            if (pin.id == pinId) return &pin;
        }
    }
    return nullptr;
}

Node* Stargraph::FindNodeByPinId(int pinId) {
    for (Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) return &node;
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) return &node;
        }
    }
    return nullptr;
}

Node* Stargraph::FindNodeByType(const std::string& type) {
    for (Node& node : nodes) {
        if (node.type == type) return &node;
    }
    return nullptr;
}
