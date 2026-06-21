#include "StargraphBehavior.h"
#include "GraphExecutor.h"
#include <utility>

StargraphBehavior::StargraphBehavior(Stargraph graph) : graph(std::move(graph)) {
}

void StargraphBehavior::OnBeginPlay(Actor* owner) {
    GraphExecutor::Run(graph, "Event_BeginPlay");
}

void StargraphBehavior::OnTick(Actor* owner, float deltaTime) {
    GraphExecutor::Run(graph, "Event_Tick");
}

void StargraphBehavior::OnEndPlay(Actor* owner) {
    GraphExecutor::Run(graph, "Event_EndPlay");
}
