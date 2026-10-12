#pragma once

#include <string>

struct ExecutionContext;
class Stargraph;
struct Node;
struct Variable;

void RegisterVariableNodes();
Variable* ResolveVariableForNode(Node& variableNode, Stargraph& graph, ExecutionContext& ctx);