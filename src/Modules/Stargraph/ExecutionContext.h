#pragma once

class Actor;

struct ExecutionContext {
    Actor* actor = nullptr;
    float deltaTime = 0.0f;

    int enteredExecPin = 0;
};