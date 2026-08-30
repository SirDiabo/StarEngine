#include "Nodes_Functions.h"
#include "NodeRegistry.h"
#include "GraphExecutor.h"
#include "Runtime/Actor.h"

void RegisterFunctionNodes() {
    NodeTemplate functionEntry;
    functionEntry.type = "Function_Entry";
    functionEntry.category = "Functions";
    functionEntry.moduleName = "Stargraph";
    functionEntry.isEvent = true;
    functionEntry.hideFromContextMenu = true;
    functionEntry.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(functionEntry);

    NodeTemplate returnNode;
    returnNode.type = "Return";
    returnNode.category = "Functions";
    returnNode.moduleName = "Stargraph";
    returnNode.hideFromContextMenu = true;
    returnNode.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
    };
    NodeRegistry::Get().RegisterNode(returnNode);

    NodeTemplate callFunction;
    callFunction.type = "Call_Function";
    callFunction.category = "Functions";
    callFunction.moduleName = "Stargraph";
    callFunction.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    callFunction.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;

        std::string functionName;
        std::vector<std::pair<std::string, std::string>> args;
        for (const Pin& p : node.inputs) {
            if (p.name == "Function Name") functionName = p.defaultValue;
            else if (p.name != "In") args.emplace_back(p.name, p.defaultValue);
        }
        if (functionName.empty()) return;

        Stargraph& classGraph = ctx.rootGraph ? *ctx.rootGraph : graph;
        std::vector<std::pair<std::string, std::string>> results;
        if (!GraphExecutor::RunFunction(classGraph, functionName, ctx, args, results)) return;

        for (const auto& result : results) {
            for (Pin& p : node.outputs) {
                if (p.name == result.first) { p.defaultValue = result.second; break; }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(callFunction);
}
