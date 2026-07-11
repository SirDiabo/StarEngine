#include "StargraphBehavior.h"
#include "GraphExecutor.h"
#include <utility>

StargraphBehavior::StargraphBehavior(Stargraph graph) : graph(std::move(graph)) {
}

void StargraphBehavior::OnBeginPlay(Actor* owner) {
    ExecutionContext ctx;
    ctx.actor = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::Run(graph, "Event_BeginPlay", ctx);
}

void StargraphBehavior::OnTick(Actor* owner, float deltaTime) {
    ExecutionContext ctx;
    ctx.actor = owner;
    ctx.deltaTime = deltaTime;
    GraphExecutor::Run(graph, "Event_Tick", ctx);
}

void StargraphBehavior::OnEndPlay(Actor* owner) {
    ExecutionContext ctx;
    ctx.actor = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::Run(graph, "Event_EndPlay", ctx);
}

void StargraphBehavior::OnCustomEvent(Actor* owner, const std::string& eventName,
                                       const std::vector<std::pair<std::string, std::string>>& params) {
    ExecutionContext ctx;
    ctx.actor = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::RunCustomEvent(graph, eventName, ctx, params);
}