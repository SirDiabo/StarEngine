#pragma once

class Actor;

class GraphBehavior {
public:
    virtual ~GraphBehavior() = default;

    virtual void OnBeginPlay(Actor* owner) = 0;
    virtual void OnTick(Actor* owner, float deltaTime) = 0;
    virtual void OnEndPlay(Actor* owner) = 0;
};
