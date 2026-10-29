#pragma once
#include "Runtime/ActorBehavior.h"
#include "Runtime/WidgetBehavior.h"
#include "Stargraph.h"

class StargraphBehavior : public ActorBehavior, public WidgetBehavior {
public:
    explicit StargraphBehavior(Stargraph graph);
    ~StargraphBehavior() override;

    void OnBeginPlay(Actor* owner) override;
    void OnTick(Actor* owner, float deltaTime) override;
    void OnEndPlay(Actor* owner) override;
    void OnCustomEvent(Actor* owner, const std::string& eventName,
                        const std::vector<std::pair<std::string, std::string>>& params) override;
    void OnCustomEvent(Widget* owner, const std::string& eventName,
                        const std::vector<std::pair<std::string, std::string>>& params);
    bool CallWidgetEvent(Widget* owner, const std::string& eventName, const CallParams& params) override;
    bool CallWidgetFunction(Widget* owner, const std::string& functionName, const CallParams& args, std::string& result) override;
    bool SetWidgetVariable(Widget* owner, const std::string& name, const std::string& value) override;
    bool GetWidgetVariable(Widget* owner, const std::string& name, std::string& value) override;
    void OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                           const std::vector<std::pair<std::string, std::string>>& params) override;
    void OnDestroyed(Actor* owner) override;
    void CollectSavableVariables(std::vector<SavableVariable>& out) override;
    void ApplySavedVariable(const SavableVariable& saved) override;
    void VisitReplicatedVariables(const ReplicatedVariableVisitor& visitor) override;
    bool FindReplicatedVariable(const std::string& name, ReplicationMode& outMode) override;
    void SetReplicatedVariable(const std::string& name, const std::string& value) override;
    bool FindEventReplication(const std::string& eventName, ReplicationMode& outMode) override;

    void OnConstruct(Widget* owner) override;
    void OnTick(Widget* owner, float deltaTime) override;
    void OnDestruct(Widget* owner) override;
    void OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) override;

    Stargraph& GetGraph() { return graph; }

private:
    Stargraph graph;
};
