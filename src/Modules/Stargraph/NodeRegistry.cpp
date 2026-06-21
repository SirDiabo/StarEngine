#include "NodeRegistry.h"

NodeRegistry& NodeRegistry::Get() {
    static NodeRegistry instance;
    return instance;
}

void NodeRegistry::RegisterNode(const NodeTemplate& nodeTemplate) {
    templates[nodeTemplate.type] = nodeTemplate;
}

const NodeTemplate* NodeRegistry::GetTemplate(const std::string& type) {
    auto it = templates.find(type);
    return it != templates.end() ? &it->second : nullptr;
}