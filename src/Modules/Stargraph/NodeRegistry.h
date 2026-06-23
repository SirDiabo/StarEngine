#pragma once
#include "Stargraph.h"
#include <functional>
#include <unordered_map>

struct NodeTemplate {
    std::string type;
    std::string category;
    bool isEvent = false;
    std::function<void(Node*)> createPins;
    std::function<void(Node&, Stargraph&)> execute;
};

class NodeRegistry {
public:
    static NodeRegistry& Get();

    void RegisterNode(const NodeTemplate& nodeTemplate);
    const NodeTemplate* GetTemplate(const std::string& type);
    const std::unordered_map<std::string, NodeTemplate>& GetAllTemplates() const { return templates; }

private:
    std::unordered_map<std::string, NodeTemplate> templates;
};
