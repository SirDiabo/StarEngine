#pragma once
#include "ExecutionContext.h"
#include <string>
#include <vector>
#include <utility>

class Stargraph;
struct Node;

class GraphExecutor {
public:
    static void Run(Stargraph& graph, const std::string& eventType, ExecutionContext ctx = {});
    static void PropagateDataPins(Stargraph& graph, Node* node, ExecutionContext& ctx);
    static void ExecuteLinkedNodes(Stargraph& graph, Node* startNode, ExecutionContext& ctx, int enteredPinId = 0);

    static void RunCustomEvent(Stargraph& graph, const std::string& eventName, ExecutionContext ctx,
                                const std::vector<std::pair<std::string, std::string>>& params = {});

    static bool RunFunction(Stargraph& classGraph, const std::string& functionName, ExecutionContext ctx,
                             const std::vector<std::pair<std::string, std::string>>& args,
                             std::vector<std::pair<std::string, std::string>>& outResults);

    static void RunFromPin(Stargraph& graph, Node* node, ExecutionContext& ctx, const std::string& pinName);

    // Checks which pin fired.
    static bool EnteredViaPin(const Node& node, const ExecutionContext& ctx, const std::string& pinName);

private:
    static Node* FollowExecLink(Stargraph& graph, Node* fromNode, const std::string& pinName = "Out", int* outPinId = nullptr);
    static void ExecuteFromNode(Stargraph& graph, Node* startNode, ExecutionContext& ctx);
};
