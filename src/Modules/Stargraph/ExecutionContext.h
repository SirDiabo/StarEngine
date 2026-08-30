#pragma once

class Actor;
class Stargraph;

struct ExecutionContext {
    Actor* actor = nullptr;
    float deltaTime = 0.0f;

    int enteredExecPin = 0;

    Stargraph* rootGraph = nullptr;

    int callDepth = 0;
};
