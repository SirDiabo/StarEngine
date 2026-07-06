#include "NodeRegistry.h"
#include "Core/Module.h"

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

std::vector<NodeTemplate> NodeRegistry::GetFilteredTemplates(bool includeContextMenuHidden, const std::string& componentClass) const {
    std::vector<NodeTemplate> result;
    for (const auto& kv : templates) {
        if (!componentClass.empty() && !kv.second.requiresComponentClass.empty() && kv.second.requiresComponentClass != componentClass) continue;
        if (kv.second.moduleName.empty()) {
            result.push_back(kv.second);
            continue;
        }
        if (ModuleManager::Get().IsModuleEnabled(kv.second.moduleName)) {
            result.push_back(kv.second);
        }
    }
    if (!includeContextMenuHidden) {
        result.erase(std::remove_if(result.begin(), result.end(),
            [](const NodeTemplate& t) { return t.hideFromContextMenu; }), result.end());
    }
    return result;
}