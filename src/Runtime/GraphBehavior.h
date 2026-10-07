#pragma once
#include <string>
#include <utility>
#include <vector>

class Actor;
class Component;

class GraphBehavior {
public:
    virtual ~GraphBehavior() = default;

    virtual void OnBeginPlay(Actor* owner) = 0;
    virtual void OnTick(Actor* owner, float deltaTime) = 0;
    virtual void OnEndPlay(Actor* owner) = 0;

    virtual void OnCustomEvent(Actor* owner, const std::string& eventName,
                                const std::vector<std::pair<std::string, std::string>>& params) {}

    virtual void OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                                   const std::vector<std::pair<std::string, std::string>>& params) {}

    virtual void OnDestroyed(Actor* owner) {}
};