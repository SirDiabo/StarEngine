#include "Nodes_Flow.h"
#include "NodeRegistry.h"
#include "GraphExecutor.h"
#include "Runtime/Networking/NetworkBackend.h"
#include <algorithm>
#include <cctype>
#include <charconv>

static bool ParseStrictInt(const std::string& text, int& out) {
    size_t begin = 0, end = text.size();
    while (begin < end && isspace((unsigned char)text[begin])) begin++;
    while (end > begin && isspace((unsigned char)text[end - 1])) end--;
    if (begin == end) return false;
    int value = 0;
    auto result = std::from_chars(text.data() + begin, text.data() + end, value);
    if (result.ec != std::errc() || result.ptr != text.data() + end) return false;
    out = value;
    return true;
}

static bool ParseIntRange(const std::string& text, int& lo, int& hi) {
    int single = 0;
    if (ParseStrictInt(text, single)) { lo = hi = single; return true; }

    auto trySplit = [&](size_t pos, size_t separatorLength) {
        int a = 0, b = 0;
        if (!ParseStrictInt(text.substr(0, pos), a)) return false;
        if (!ParseStrictInt(text.substr(pos + separatorLength), b)) return false;
        lo = std::min(a, b);
        hi = std::max(a, b);
        return true;
    };

    size_t pos = text.find("..");
    if (pos != std::string::npos && trySplit(pos, 2)) return true;
    pos = text.find(" to ");
    if (pos != std::string::npos && trySplit(pos, 4)) return true;
    for (size_t i = 1; i < text.size(); i++) {
        if (text[i] == '-' && trySplit(i, 1)) return true;
    }
    return false;
}

static bool NormalizeIntCase(const std::string& input, std::string& spec) {
    int lo = 0, hi = 0;
    if (!ParseIntRange(input, lo, hi)) return false;
    spec = (lo == hi) ? std::to_string(lo) : std::to_string(lo) + " to " + std::to_string(hi);
    return true;
}

static std::string StringCasePinName(const std::string& spec) {
    std::string visible;
    for (char c : spec) {
        if (c == '\n') visible += "\\n";
        else if (c == '\r') visible += "\\r";
        else visible += c;
    }
    return "\"" + visible + "\"";
}

void RegisterFlowNodes() {
    NodeTemplate branch;
    branch.type = "Branch";
    branch.category = "Flow";
    branch.moduleName = "Stargraph";
    branch.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"True",  PinType::Exec, false, true});
        node->outputs.push_back({"False", PinType::Exec, false, true});
    };
    branch.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool condition = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Condition") condition = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        GraphExecutor::RunFromPin(graph, &node, ctx, condition ? "True" : "False");
    };
    NodeRegistry::Get().RegisterNode(branch);

    NodeTemplate hasAuthority;
    hasAuthority.type = "Has_Authority";
    hasAuthority.category = "Flow";
    hasAuthority.moduleName = "Stargraph";
    hasAuthority.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Authority", PinType::Exec, false, true});
        node->outputs.push_back({"Remote",    PinType::Exec, false, true});
    };
    hasAuthority.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool authority = NetworkBackendRegistry::Get().HasAuthority();
        GraphExecutor::RunFromPin(graph, &node, ctx, authority ? "Authority" : "Remote");
    };
    NodeRegistry::Get().RegisterNode(hasAuthority);

    NodeTemplate forLoop;
    forLoop.type = "For_Loop";
    forLoop.category = "Flow";
    forLoop.moduleName = "Stargraph";
    forLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec,    true,  false});
        node->inputs.push_back({"First", PinType::Integer, true,  false, 0, "0"});
        node->inputs.push_back({"Last",  PinType::Integer, true,  false, 0, "9"});
        node->outputs.push_back({"Loop Body", PinType::Exec,    false, true});
        node->outputs.push_back({"Index",     PinType::Integer, false, true});
        node->outputs.push_back({"Completed", PinType::Exec,    false, true});
    };
    forLoop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int first = 0, last = 9;
        for (const Pin& p : node.inputs) {
            if (p.name == "First" && !p.defaultValue.empty()) first = p.defaultValue.AsInt();
            if (p.name == "Last" && !p.defaultValue.empty()) last = p.defaultValue.AsInt();
        }
        for (int i = first; i <= last; i++) {
            char buf[64]; snprintf(buf, sizeof(buf), "%d", i);
            for (Pin& p : node.outputs) if (p.name == "Index") p.defaultValue = buf;
            GraphExecutor::RunFromPin(graph, &node, ctx, "Loop Body");
        }
        GraphExecutor::RunFromPin(graph, &node, ctx, "Completed");
    };
    NodeRegistry::Get().RegisterNode(forLoop);

    NodeTemplate whileLoop;
    whileLoop.type = "While_Loop";
    whileLoop.category = "Flow";
    whileLoop.moduleName = "Stargraph";
    whileLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"Loop Body", PinType::Exec, false, true});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    whileLoop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int loopLimit = 1000;
        while (true) {
            GraphExecutor::PropagateDataPins(graph, &node, ctx);
            bool condition = false;
            for (const Pin& p : node.inputs) {
                if (p.name == "Condition") { condition = (p.defaultValue == "true" || p.defaultValue == "1"); break; }
            }
            if (!condition) break;
            GraphExecutor::RunFromPin(graph, &node, ctx, "Loop Body");
            if (--loopLimit <= 0) break;
        }
        GraphExecutor::RunFromPin(graph, &node, ctx, "Completed");
    };
    NodeRegistry::Get().RegisterNode(whileLoop);

    NodeTemplate sequence;
    sequence.type = "Sequence";
    sequence.category = "Flow";
    sequence.moduleName = "Stargraph";
    sequence.dynamicExecOutputs = true;
    sequence.dynamicExecInitialCases = {"", ""};
    sequence.dynamicExecLegacyCases = {"", "", ""};
    sequence.dynamicExecMinCases = 1;
    sequence.dynamicExecHeader = "Outputs";
    sequence.dynamicExecAddLabel = "+ Add Output";
    sequence.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
    };
    sequence.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (const Pin& outPin : node.outputs) {
            if (outPin.type != PinType::Exec) continue;
            for (Link* lk : graph.GetLinksFromPin(outPin.id)) {
                GraphExecutor::ExecuteLinkedNodes(graph, graph.FindNodeByPinId(lk->inputPinId), ctx, lk->inputPinId);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(sequence);

    NodeTemplate selectFromInt;
    selectFromInt.type = "Select_From_Int";
    selectFromInt.menuLabel = "Select From Int";
    selectFromInt.category = "Flow";
    selectFromInt.moduleName = "Stargraph";
    selectFromInt.dynamicExecOutputs = true;
    selectFromInt.dynamicExecFixedPin = "Default";
    selectFromInt.dynamicExecHeader = "Cases";
    selectFromInt.dynamicExecAddLabel = "+ Add Case";
    selectFromInt.dynamicExecPlaceholder = "5 or 1 to 10";
    selectFromInt.normalizeExecCase = NormalizeIntCase;
    selectFromInt.execCasePinName = [](const std::string& spec, int) { return spec; };
    selectFromInt.newExecCaseDefault = [](const std::vector<std::string>& existing) {
        int next = 0;
        bool any = false;
        for (const std::string& spec : existing) {
            int lo = 0, hi = 0;
            if (ParseIntRange(spec, lo, hi)) { next = any ? std::max(next, hi + 1) : hi + 1; any = true; }
        }
        return std::to_string(next);
    };
    selectFromInt.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Value", PinType::Integer, true, false, 0, "0"});
    };
    selectFromInt.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int value = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") value = p.defaultValue.AsInt();
        for (const std::string& spec : node.execCases) {
            int lo = 0, hi = 0;
            if (ParseIntRange(spec, lo, hi) && value >= lo && value <= hi) {
                GraphExecutor::RunFromPin(graph, &node, ctx, spec);
                return;
            }
        }
        GraphExecutor::RunFromPin(graph, &node, ctx, "Default");
    };
    NodeRegistry::Get().RegisterNode(selectFromInt);

    NodeTemplate selectFromString;
    selectFromString.type = "Select_From_String";
    selectFromString.menuLabel = "Select From String";
    selectFromString.category = "Flow";
    selectFromString.moduleName = "Stargraph";
    selectFromString.dynamicExecOutputs = true;
    selectFromString.dynamicExecFixedPin = "Default";
    selectFromString.dynamicExecHeader = "Cases";
    selectFromString.dynamicExecAddLabel = "+ Add Case";
    selectFromString.dynamicExecPlaceholder = "Text to match";
    selectFromString.normalizeExecCase = [](const std::string& input, std::string& spec) { spec = input; return true; };
    selectFromString.execCasePinName = [](const std::string& spec, int) { return StringCasePinName(spec); };
    selectFromString.newExecCaseDefault = [](const std::vector<std::string>& existing) {
        for (int n = (int)existing.size();; n++) {
            std::string candidate = "Case " + std::to_string(n);
            if (std::find(existing.begin(), existing.end(), candidate) == existing.end()) return candidate;
        }
    };
    selectFromString.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Value", PinType::String, true, false, 0, ""});
    };
    selectFromString.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string value;
        for (const Pin& p : node.inputs) if (p.name == "Value") value = p.defaultValue;
        for (const std::string& spec : node.execCases) {
            if (spec == value) {
                GraphExecutor::RunFromPin(graph, &node, ctx, StringCasePinName(spec));
                return;
            }
        }
        GraphExecutor::RunFromPin(graph, &node, ctx, "Default");
    };
    NodeRegistry::Get().RegisterNode(selectFromString);

    NodeTemplate doOnce;
    doOnce.type = "Do_Once";
    doOnce.category = "Flow";
    doOnce.moduleName = "Stargraph";
    doOnce.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Reset", PinType::Exec, false, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    doOnce.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (GraphExecutor::EnteredViaPin(node, ctx, "Reset")) {
            node.flowState = false;
            return;
        }

        if (!node.flowState) {
            GraphExecutor::RunFromPin(graph, &node, ctx, "Out");
            node.flowState = true;
        }
    };
    NodeRegistry::Get().RegisterNode(doOnce);

    NodeTemplate gate;
    gate.type = "Gate";
    gate.category = "Flow";
    gate.moduleName = "Stargraph";
    gate.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec, true, false});
        node->inputs.push_back({"Open",      PinType::Exec, false, false});
        node->inputs.push_back({"Close",     PinType::Exec, false, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    gate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (GraphExecutor::EnteredViaPin(node, ctx, "Open"))  { node.flowState = true;  return; }
        if (GraphExecutor::EnteredViaPin(node, ctx, "Close")) { node.flowState = false; return; }

        if (node.flowState) {
            GraphExecutor::RunFromPin(graph, &node, ctx, "Out");
        }
    };
    NodeRegistry::Get().RegisterNode(gate);

    NodeTemplate flipFlop;
    flipFlop.type = "Flip_Flop";
    flipFlop.category = "Flow";
    flipFlop.moduleName = "Stargraph";
    flipFlop.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"True",  PinType::Exec, false, true});
        node->outputs.push_back({"False", PinType::Exec, false, true});
    };
    flipFlop.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        node.flowState = !node.flowState;

        GraphExecutor::RunFromPin(graph, &node, ctx, node.flowState ? "True" : "False");
    };
    NodeRegistry::Get().RegisterNode(flipFlop);
}