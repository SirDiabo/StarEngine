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
    bool latent = false;
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
    std::unordered_map<std::string, std::string> legacyOutputPinAliases;
    std::string dynamicWildcardInputPrefix;
    int dynamicWildcardInputInitial = 0;
    bool dynamicWildcardInputsNumeric = false;
    std::string modePinName;
    std::vector<std::string> modeOptions;
    std::function<void(Stargraph&, Node&)> normalizePins;
    std::function<bool(const Node&)> dynamicInputsEnabled;
    std::function<void(Node*)> createPins;
    std::function<void(Node&, Stargraph&, ExecutionContext&)> execute;
};

inline bool HasDynamicWildcardInputs(const NodeTemplate& t) {
    return !t.dynamicWildcardInputPrefix.empty() || t.dynamicWildcardInputsNumeric;
}

inline bool IsDynamicWildcardInputName(const NodeTemplate& t, const std::string& name) {
    if (t.dynamicWildcardInputsNumeric) {
        if (name.empty()) return false;
        for (char c : name) if (c < '0' || c > '9') return false;
        return true;
    }
    return !t.dynamicWildcardInputPrefix.empty() && name.rfind(t.dynamicWildcardInputPrefix, 0) == 0;
}

inline std::string DynamicWildcardInputName(const NodeTemplate& t, int index) {
    return (t.dynamicWildcardInputsNumeric ? std::string() : t.dynamicWildcardInputPrefix) + std::to_string(index);
}

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