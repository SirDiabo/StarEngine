#pragma once
#include "ExecutionContext.h"
#include <string>

class Stargraph;
struct Node;

class GraphExecutor {
public:
    static void Run(Stargraph& graph, const std::string& eventType, ExecutionContext ctx = {});
    static void PropagateDataPins(Stargraph& graph, Node* node, ExecutionContext& ctx);

private:
    static Node* FollowExecLink(Stargraph& graph, Node* fromNode, const std::string& pinName = "Out");
};