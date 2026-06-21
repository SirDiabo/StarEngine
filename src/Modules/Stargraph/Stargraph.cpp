#include "Stargraph.h"

Node* Stargraph::CreateNode(const std::string& type) {
    Node node;
    node.id = std::to_string(nodes.size());
    node.type = type;
    node.title = type;
    nodes.push_back(node);
    return &nodes.back();
}

void Stargraph::AddLink(const Link& link) {
    links.push_back(link);
}

void Stargraph::AddVariable(const Variable& var) {
    variables.push_back(var);
}