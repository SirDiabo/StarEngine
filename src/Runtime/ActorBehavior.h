#pragma once
#include "ReplicationMode.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

class Actor;
class Component;

struct SavableVariable {
    std::string name;
    std::string saveFile;
    bool isArray = false;
    std::string value;
    std::vector<std::string> arrayValues;
};

using ReplicatedVariableVisitor = std::function<void(const std::string& name, const std::string& value, ReplicationMode mode)>;

class ActorBehavior {
public:
    virtual ~ActorBehavior() = default;

    virtual void OnBeginPlay(Actor* owner) = 0;
    virtual void OnTick(Actor* owner, float deltaTime) = 0;
    virtual void OnEndPlay(Actor* owner) = 0;

    virtual void OnCustomEvent(Actor* owner, const std::string& eventName,
                                const std::vector<std::pair<std::string, std::string>>& params) {}

    virtual void OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                                   const std::vector<std::pair<std::string, std::string>>& params) {}

    virtual void OnDestroyed(Actor* owner) {}

    virtual void CollectSavableVariables(std::vector<SavableVariable>& out) {}
    virtual void ApplySavedVariable(const SavableVariable& saved) {}

    virtual void VisitReplicatedVariables(const ReplicatedVariableVisitor& visitor) {}
    virtual bool FindReplicatedVariable(const std::string& name, ReplicationMode& outMode) { return false; }
    virtual void SetReplicatedVariable(const std::string& name, const std::string& value) {}
    virtual bool FindEventReplication(const std::string& eventName, ReplicationMode& outMode) { return false; }
};
