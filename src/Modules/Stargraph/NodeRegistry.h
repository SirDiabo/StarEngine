#pragma once
#include "Stargraph.h"
#include "ExecutionContext.h"
#include <functional>
#include <unordered_map>

struct NodeTemplate {
    std::string type;
    std::string category;
    std::string moduleName;
    bool isEvent = false;
    bool hideFromContextMenu = false;
    std::string menuLabel;
    std::string boundComponentType;
    std::string boundComponentEvent;
    std::string boundElementClass;
    bool dynamicExecOutputs = false;
    std::string dynamicExecFixedPin;
    std::vector<std::string> dynamicExecInitialCases;
    std::vector<std::string> dynamicExecLegacyCases;
    int dynamicExecMinCases = 0;
    std::string dynamicExecHeader;
    std::string dynamicExecAddLabel;
    std::string dynamicExecPlaceholder;
    std::function<bool(const std::string&, std::string&)> normalizeExecCase;
    std::function<std::string(const std::string&, int)> execCasePinName;
    std::function<std::string(const std::vector<std::string>&)> newExecCaseDefault;
    std::unordered_map<std::string, std::string> legacyInputPinAliases;
    std::string dynamicWildcardInputPrefix;
    int dynamicWildcardInputInitial = 0;
    std::function<void(Node*)> createPins;
    std::function<void(Node&, Stargraph&, ExecutionContext&)> execute;
};

class NodeRegistry {
public:
    static NodeRegistry& Get();

    void RegisterNode(const NodeTemplate& nodeTemplate);
    const NodeTemplate* GetTemplate(const std::string& type);
    std::vector<NodeTemplate> GetFilteredTemplates(bool includeContextMenuHidden = true) const;
    const std::unordered_map<std::string, NodeTemplate>& GetAllTemplates() const { return templates; }

private:
    std::unordered_map<std::string, NodeTemplate> templates;
};