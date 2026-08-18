#include "Nodes_Timers.h"
#include "NodeRegistry.h"
#include "StargraphBehavior.h"
#include "TimerManager.h"
#include "Runtime/Actor.h"
#include <cstdlib>
#include "ObjectPinRef.h"

static StargraphBehavior* ResolveOwner(ExecutionContext& ctx) {
    if (!ctx.actor) return nullptr;
    return dynamic_cast<StargraphBehavior*>(ctx.actor->GetGraphBehavior());
}

static Actor* ResolveTarget(Node& node, ExecutionContext& ctx) {
    Actor* target = ctx.actor;
    for (const Pin& p : node.inputs) {
        if (p.name == "Target" && !p.defaultValue.empty()) {
            target = ObjectPinRef::ResolveAs<Actor>(node, p.defaultValue, "Actor", ctx);
            break;
        }
    }
    return target;
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
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    delay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        if (TimerManager::Get().IsNodeTimerActive(owner, &node)) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Completed", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(delay);

    NodeTemplate retriggerableDelay;
    retriggerableDelay.type = "Retriggerable_Delay";
    retriggerableDelay.category = "Timers";
    retriggerableDelay.moduleName = "Stargraph";
    retriggerableDelay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Duration", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    retriggerableDelay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().CancelNodeTimers(owner, &node);
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Completed", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(retriggerableDelay);

    NodeTemplate stackableDelay;
    stackableDelay.type = "Stackable_Delay";
    stackableDelay.category = "Timers";
    stackableDelay.moduleName = "Stargraph";
    stackableDelay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Duration", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    stackableDelay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Completed", ReadDuration(node));
    };
    NodeRegistry::Get().RegisterNode(stackableDelay);

    NodeTemplate delayNextFrame;
    delayNextFrame.type = "Delay_Until_Next_Frame";
    delayNextFrame.category = "Timers";
    delayNextFrame.moduleName = "Stargraph";
    delayNextFrame.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    delayNextFrame.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        StargraphBehavior* owner = ResolveOwner(ctx);
        if (!owner) return;
        TimerManager::Get().StartNodeTimer(owner, ctx.actor, &node, "Completed", ctx.deltaTime);
    };
    NodeRegistry::Get().RegisterNode(delayNextFrame);

    NodeTemplate timerByName;
    timerByName.type = "Timer_By_Function_Name";
    timerByName.category = "Timers";
    timerByName.moduleName = "Stargraph";
    timerByName.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Rate", PinType::Float, true, false, 0, "1.0"});
        node->inputs.push_back({"Loop", PinType::Boolean, true, false, 0, "false"});
        node->inputs.push_back({"Limit to Frame Rate", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    timerByName.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ResolveTarget(node, ctx);
        if (!target) return;
        StargraphBehavior* owner = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior());
        if (!owner) return;

        std::string functionName;
        float rate = 1.0f;
        bool loop = false;
        bool limitToFrameRate = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Function Name") functionName = p.defaultValue;
            else if (p.name == "Rate" && !p.defaultValue.empty()) rate = p.defaultValue.AsFloat();
            else if (p.name == "Loop") loop = (p.defaultValue == "true");
            else if (p.name == "Limit to Frame Rate") limitToFrameRate = (p.defaultValue == "true");
        }
        if (functionName.empty()) return;

        TimerManager::Get().StartNamedTimer(owner, target, functionName, rate, loop, limitToFrameRate);
    };
    NodeRegistry::Get().RegisterNode(timerByName);

    NodeTemplate pauseTimer;
    pauseTimer.type = "Pause_Timer_By_Function_Name";
    pauseTimer.category = "Timers";
    pauseTimer.moduleName = "Stargraph";
    pauseTimer.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    pauseTimer.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ResolveTarget(node, ctx);
        if (!target) return;
        StargraphBehavior* owner = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior());
        if (!owner) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Function Name") TimerManager::Get().PauseNamedTimer(owner, p.defaultValue);
        }
    };
    NodeRegistry::Get().RegisterNode(pauseTimer);

    NodeTemplate resumeTimer;
    resumeTimer.type = "Resume_Timer_By_Function_Name";
    resumeTimer.category = "Timers";
    resumeTimer.moduleName = "Stargraph";
    resumeTimer.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    resumeTimer.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ResolveTarget(node, ctx);
        if (!target) return;
        StargraphBehavior* owner = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior());
        if (!owner) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Function Name") TimerManager::Get().ResumeNamedTimer(owner, p.defaultValue);
        }
    };
    NodeRegistry::Get().RegisterNode(resumeTimer);

    NodeTemplate clearTimer;
    clearTimer.type = "Clear_Timer_By_Function_Name";
    clearTimer.category = "Timers";
    clearTimer.moduleName = "Stargraph";
    clearTimer.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    clearTimer.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ResolveTarget(node, ctx);
        if (!target) return;
        StargraphBehavior* owner = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior());
        if (!owner) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Function Name") TimerManager::Get().ClearNamedTimer(owner, p.defaultValue);
        }
    };
    NodeRegistry::Get().RegisterNode(clearTimer);
}