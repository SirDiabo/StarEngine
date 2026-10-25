#pragma once
#include "Runtime/ActorBehavior.h"
#include "StarscriptRuntime.h"

namespace Starscript {

class StarscriptBehavior : public ActorBehavior {
public:
    explicit StarscriptBehavior(std::shared_ptr<Program> program);

    void OnBeginPlay(Actor* owner) override;
    void OnTick(Actor* owner, float deltaTime) override;
    void OnEndPlay(Actor* owner) override;
    void OnDestroyed(Actor* owner) override;
    void OnCustomEvent(Actor* owner, const std::string& eventName, const std::vector<std::pair<std::string, std::string>>& params) override;
    void OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                          const std::vector<std::pair<std::string, std::string>>& params) override;

    void CollectSavableVariables(std::vector<SavableVariable>& out) override;
    void ApplySavedVariable(const SavableVariable& saved) override;

    void VisitReplicatedVariables(const ReplicatedVariableVisitor& visitor) override;
    bool FindReplicatedVariable(const std::string& name, ReplicationMode& outMode) override;
    void SetReplicatedVariable(const std::string& name, const std::string& value) override;
    bool FindEventReplication(const std::string& eventName, ReplicationMode& outMode) override;

    Instance& GetInstance() { return instance; }

private:
    void ReportCompileErrors();

    std::shared_ptr<Program> program;
    Instance instance;
    bool reportedCompileErrors = false;
};

}
