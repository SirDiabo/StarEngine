#pragma once

#include <vector>

class Actor;
class Stargraph;
class Widget;

struct ExecutionContext {
    Actor* actor = nullptr;
    Widget* widget = nullptr;
    float deltaTime = 0.0f;

    int enteredExecPin = 0;

    Stargraph* rootGraph = nullptr;

    int callDepth = 0;

    std::vector<int> executedNodeIds;
};
