#pragma once
#include <string>

class Stargraph;
struct Node;

class GraphExecutor {
public:
    static void Run(Stargraph& graph, const std::string& eventType);

private:
    static Node* FollowExecLink(Stargraph& graph, Node* fromNode);
};
