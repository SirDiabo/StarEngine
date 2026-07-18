#pragma once
#include "Runtime/GraphBehavior.h"
#include "Stargraph.h"

class StargraphBehavior : public GraphBehavior {
public:
    explicit StargraphBehavior(Stargraph graph);

    void OnBeginPlay(Actor* owner) override;
    void OnTick(Actor* owner, float deltaTime) override;
    void OnEndPlay(Actor* owner) override;
    void OnCustomEvent(Actor* owner, const std::string& eventName,
                        const std::vector<std::pair<std::string, std::string>>& params) override;
    void OnDestroyed(Actor* owner) override;

    Stargraph& GetGraph() { return graph; }

private:
    Stargraph graph;
};