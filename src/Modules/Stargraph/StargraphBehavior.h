#pragma once
#include "Runtime/GraphBehavior.h"
#include "Stargraph.h"

class StargraphBehavior : public GraphBehavior {
public:
    explicit StargraphBehavior(Stargraph graph);

    void OnBeginPlay(Actor* owner) override;
    void OnTick(Actor* owner, float deltaTime) override;
    void OnEndPlay(Actor* owner) override;

    Stargraph& GetGraph() { return graph; }

private:
    Stargraph graph;
};
