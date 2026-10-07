#pragma once
#include "Runtime/GraphBehavior.h"
#include "Runtime/WidgetBehavior.h"
#include "Stargraph.h"

class StargraphBehavior : public GraphBehavior, public WidgetBehavior {
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
    void OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                           const std::vector<std::pair<std::string, std::string>>& params) override;
    void OnDestroyed(Actor* owner) override;

    void OnConstruct(Widget* owner) override;
    void OnTick(Widget* owner, float deltaTime) override;
    void OnDestruct(Widget* owner) override;
    void OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) override;

    Stargraph& GetGraph() { return graph; }

private:
    Stargraph graph;
};