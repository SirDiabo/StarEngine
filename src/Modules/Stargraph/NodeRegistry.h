#pragma once
#include "Stargraph.h"
#include <functional>

struct NodeTemplate {
    std::string type;
    std::string category;
    std::function<void(Node*)> createPins;
};

class NodeRegistry {
public:
    static NodeRegistry& Get();

    void RegisterNode(const NodeTemplate& nodeTemplate);
    const NodeTemplate* GetTemplate(const std::string& type);

private:
    std::unordered_map<std::string, NodeTemplate> templates;
};