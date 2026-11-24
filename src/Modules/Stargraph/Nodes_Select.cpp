#include "Nodes_Select.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr const char* kModePin = "Mode";
constexpr const char* kIndexPin = "Index";
constexpr const char* kResultPin = "Return Value";
constexpr const char* kBoolMode = "Bool";
constexpr const char* kIntMode = "Int";
constexpr int kMaxOptions = 256;

bool IsNumericName(const std::string& name) {
    if (name.empty() || name.size() > 6) return false;
    for (char c : name) if (c < '0' || c > '9') return false;
    return true;
}

Pin* FindInput(Node& node, const std::string& name) {
    for (Pin& pin : node.inputs) if (pin.name == name) return &pin;
    return nullptr;
}

const Pin* FindInput(const Node& node, const std::string& name) {
    for (const Pin& pin : node.inputs) if (pin.name == name) return &pin;
    return nullptr;
}

std::string ModeOf(const Node& node) {
    const Pin* mode = FindInput(node, kModePin);
    if (mode && (const std::string&)mode->defaultValue == kIntMode) return kIntMode;
    return kBoolMode;
}

bool LooksLikeBool(const std::string& v) { return v == "true" || v == "false" || v == "1" || v == "0"; }

bool LooksLikeInt(const std::string& v) {
    if (v.empty() || v.size() > 10) return false;
    size_t i = (v[0] == '-') ? 1 : 0;
    if (i == v.size()) return false;
    for (; i < v.size(); i++) if (v[i] < '0' || v[i] > '9') return false;
    return true;
}

Pin* EnsureOption(Stargraph& graph, Node& node, const std::string& name) {
    if (Pin* existing = FindInput(node, name)) return existing;
    Pin* pin = graph.AddDynamicPin(node, false, name, PinType::Wildcard);
    if (pin) pin->isWildcard = true;
    return pin;
}

void RemoveInputsWhere(Stargraph& graph, Node& node, bool (*pred)(const std::string&)) {
    std::vector<std::string> doomed;
    for (const Pin& pin : node.inputs) if (pred(pin.name)) doomed.push_back(pin.name);
    for (const std::string& name : doomed) graph.RemoveDynamicPin(node, false, name);
}

bool IsTrueFalse(const std::string& name) { return name == "True" || name == "False"; }

int OrderKey(const Pin& pin, bool intMode) {
    if (pin.name == kModePin) return 0;
    if (pin.name == kIndexPin) return 3;
    if (!intMode && pin.name == "True") return 1;
    if (!intMode && pin.name == "False") return 2;
    if (intMode && IsNumericName(pin.name)) return 1;
    return 4;
}

void NormalizeSelect(Stargraph& graph, Node& node) {
    std::string mode = ModeOf(node);
    bool intMode = mode == kIntMode;

    if (Pin* modePin = FindInput(node, kModePin)) {
        modePin->defaultValue = mode;
        modePin->type = PinType::String;
        modePin->fieldOnly = true;
    } else if (Pin* created = graph.AddDynamicPin(node, false, kModePin, PinType::String)) {
        created->defaultValue = mode;
        created->fieldOnly = true;
    }

    if (!intMode) {
        if (!FindInput(node, "True") && FindInput(node, "1")) FindInput(node, "1")->name = "True";
        if (!FindInput(node, "False") && FindInput(node, "0")) FindInput(node, "0")->name = "False";
        RemoveInputsWhere(graph, node, IsNumericName);
        EnsureOption(graph, node, "True");
        EnsureOption(graph, node, "False");
    } else {
        bool hasNumeric = false;
        for (const Pin& pin : node.inputs) if (IsNumericName(pin.name)) { hasNumeric = true; break; }
        if (!hasNumeric) {
            if (Pin* f = FindInput(node, "False")) f->name = "0";
            if (Pin* t = FindInput(node, "True")) t->name = "1";
        }
        RemoveInputsWhere(graph, node, IsTrueFalse);

        std::vector<std::pair<long, int>> numeric;
        for (const Pin& pin : node.inputs) if (IsNumericName(pin.name)) numeric.push_back({ std::strtol(pin.name.c_str(), nullptr, 10), pin.id });
        std::sort(numeric.begin(), numeric.end());
        std::vector<int> doomed;
        for (size_t i = (size_t)kMaxOptions; i < numeric.size(); i++) doomed.push_back(numeric[i].second);
        for (int id : doomed) {
            std::string name;
            for (const Pin& pin : node.inputs) if (pin.id == id) name = pin.name;
            if (!name.empty()) graph.RemoveDynamicPin(node, false, name);
        }
        if (numeric.size() > (size_t)kMaxOptions) numeric.resize((size_t)kMaxOptions);
        for (size_t i = 0; i < numeric.size(); i++) {
            for (Pin& pin : node.inputs) if (pin.id == numeric[i].second) pin.name = "_n" + std::to_string(i);
        }
        for (Pin& pin : node.inputs) if (pin.name.rfind("_n", 0) == 0) pin.name = pin.name.substr(2);
        EnsureOption(graph, node, "0");
        EnsureOption(graph, node, "1");
    }

    PinType wanted = intMode ? PinType::Integer : PinType::Boolean;
    Pin* index = FindInput(node, kIndexPin);
    if (!index) {
        index = graph.AddDynamicPin(node, false, kIndexPin, wanted);
        if (index) index->defaultValue = intMode ? "0" : "false";
    }
    if (index) {
        if (index->type != wanted) {
            graph.RemoveLink(index->id);
            index->type = wanted;
        }
        const std::string& value = (const std::string&)index->defaultValue;
        if (intMode ? !LooksLikeInt(value) : !LooksLikeBool(value)) index->defaultValue = intMode ? "0" : "false";
        index->isWildcard = false;
        index->fieldOnly = false;
    }

    std::stable_sort(node.inputs.begin(), node.inputs.end(), [intMode](const Pin& a, const Pin& b) {
        int ka = OrderKey(a, intMode), kb = OrderKey(b, intMode);
        if (ka != kb) return ka < kb;
        if (ka == 1 && intMode) return std::strtol(a.name.c_str(), nullptr, 10) < std::strtol(b.name.c_str(), nullptr, 10);
        return false;
    });
}

void ExecuteSelect(Node& node, Stargraph&, ExecutionContext&) {
    bool intMode = ModeOf(node) == kIntMode;
    const Pin* index = FindInput(node, kIndexPin);
    std::string chosenName;
    if (!intMode) {
        bool on = false;
        if (index) {
            const std::string& v = (const std::string&)index->defaultValue;
            on = v == "true" || v == "1";
        }
        chosenName = on ? "True" : "False";
    } else {
        int count = 0;
        for (const Pin& pin : node.inputs) if (IsNumericName(pin.name)) count++;
        long picked = 0;
        if (index) picked = std::strtol(((const std::string&)index->defaultValue).c_str(), nullptr, 10);
        if (picked < 0) picked = 0;
        if (count > 0 && picked > count - 1) picked = count - 1;
        chosenName = std::to_string(picked);
    }
    const Pin* chosen = FindInput(node, chosenName);
    std::string value = chosen ? (std::string)chosen->defaultValue : std::string();
    for (Pin& out : node.outputs) if (out.name == kResultPin) out.defaultValue = value;
}

}

void RegisterSelectNode() {
    NodeTemplate t;
    t.type = "Select";
    t.menuLabel = "Select";
    t.category = "Flow";
    t.moduleName = "Stargraph";
    t.modePinName = kModePin;
    t.modeOptions = { kBoolMode, kIntMode };
    t.dynamicWildcardInputsNumeric = true;
    t.dynamicInputsEnabled = [](const Node& node) { return ModeOf(node) == kIntMode; };
    t.createPins = [](Node* node) {
        Pin mode{ kModePin, PinType::String, true, false, 0, kBoolMode };
        mode.fieldOnly = true;
        node->inputs.push_back(mode);
        Pin on{ "True", PinType::Wildcard, true, false, 0, "" };
        on.isWildcard = true;
        node->inputs.push_back(on);
        Pin off{ "False", PinType::Wildcard, true, false, 0, "" };
        off.isWildcard = true;
        node->inputs.push_back(off);
        node->inputs.push_back({ kIndexPin, PinType::Boolean, true, false, 0, "false" });
        Pin result{ kResultPin, PinType::Wildcard, false, true, 0, "" };
        result.isWildcard = true;
        node->outputs.push_back(result);
    };
    t.normalizePins = NormalizeSelect;
    t.execute = ExecuteSelect;
    NodeRegistry::Get().RegisterNode(t);
}
