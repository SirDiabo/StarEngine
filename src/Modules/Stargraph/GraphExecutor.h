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
    static void ExecuteLinkedNodes(Stargraph& graph, Node* startNode, ExecutionContext& ctx);

    static void RunCustomEvent(Stargraph& graph, const std::string& eventName, ExecutionContext ctx,
                                const std::vector<std::pair<std::string, std::string>>& params = {});

    static void RunFromPin(Stargraph& graph, Node* node, ExecutionContext& ctx, const std::string& pinName);

private:
    static Node* FollowExecLink(Stargraph& graph, Node* fromNode, const std::string& pinName = "Out");
    static void ExecuteFromNode(Stargraph& graph, Node* startNode, ExecutionContext& ctx);
};