#include "StargraphBehavior.h"
#include "GraphExecutor.h"
#include "TimerManager.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include <utility>

StargraphBehavior::StargraphBehavior(Stargraph graph) : graph(std::move(graph)) {
    this->graph.CaptureVariableDefaults();
}

StargraphBehavior::~StargraphBehavior() {
    TimerManager::Get().ClearAllTimersForOwner(this);
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

void StargraphBehavior::OnCustomEvent(Widget* owner, const std::string& eventName,
                                       const std::vector<std::pair<std::string, std::string>>& params) {
    ExecutionContext ctx;
    ctx.widget = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::RunCustomEvent(graph, eventName, ctx, params);
}

void StargraphBehavior::OnDestroyed(Actor* owner) {
    ExecutionContext ctx;
    ctx.actor = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::Run(graph, "Event_Destroyed", ctx);
}

void StargraphBehavior::OnConstruct(Widget* owner) {
    ExecutionContext ctx;
    ctx.widget = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::Run(graph, "Widget_Event_Construct", ctx);
}

void StargraphBehavior::OnTick(Widget* owner, float deltaTime) {
    ExecutionContext ctx;
    ctx.widget = owner;
    ctx.deltaTime = deltaTime;
    GraphExecutor::Run(graph, "Widget_Event_Tick", ctx);
}

void StargraphBehavior::OnDestruct(Widget* owner) {
    ExecutionContext ctx;
    ctx.widget = owner;
    ctx.deltaTime = 0.0f;
    GraphExecutor::Run(graph, "Widget_Event_Destruct", ctx);
}

void StargraphBehavior::OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) {
    if (!target) return;
    ExecutionContext ctx;
    ctx.widget = owner;
    ctx.deltaTime = 0.0f;
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        if (node.type != "Widget_Element_Event") continue;
        std::string elementName;
        for (const Pin& p : node.inputs) if (p.name == "Element") elementName = p.defaultValue;
        if (elementName != target->GetName()) continue;
        GraphExecutor::RunFromPin(graph, &node, ctx, eventName);
    }
}