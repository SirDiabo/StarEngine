#include "Nodes_Timers.h"
#include "NodeRegistry.h"
#include "StargraphBehavior.h"
#include "TimerManager.h"
#include "Runtime/Actor.h"

static StargraphBehavior* ResolveOwner(ExecutionContext& ctx) {
    if (!ctx.actor) return nullptr;
    return dynamic_cast<StargraphBehavior*>(ctx.actor->GetGraphBehavior());
}

static float ReadDuration(Node& node) {
    float duration = 1.0f;
    for (const Pin& p : node.inputs) {
        if (p.name == "Duration" && !p.defaultValue.empty()) duration = p.defaultValue.AsFloat();
    }
    return duration;
}

void RegisterTimerNodes() {
    NodeTemplate delay;
    delay.type = "Delay";
    delay.category = "Timers";
    delay.moduleName = "Stargraph";
    delay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Duration", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    delay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        if (TimerManager::Get().IsNodeTimerActive(owner, &node)) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Out", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(delay);

    NodeTemplate retriggerableDelay;
    retriggerableDelay.type = "Retriggerable_Delay";
    retriggerableDelay.category = "Timers";
    retriggerableDelay.moduleName = "Stargraph";
    retriggerableDelay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Duration", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    retriggerableDelay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().CancelNodeTimers(owner, &node);
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Out", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(retriggerableDelay);

    NodeTemplate stackableDelay;
    stackableDelay.type = "Stackable_Delay";
    stackableDelay.category = "Timers";
    stackableDelay.moduleName = "Stargraph";
    stackableDelay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Duration", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stackableDelay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Out", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(stackableDelay);

    NodeTemplate delayNextFrame;
    delayNextFrame.type = "Delay_Until_Next_Frame";
    delayNextFrame.category = "Timers";
    delayNextFrame.moduleName = "Stargraph";
    delayNextFrame.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    delayNextFrame.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Out", ctx.deltaTime);
    };
    NodeRegistry::Get().RegisterNode(delayNextFrame);
}
