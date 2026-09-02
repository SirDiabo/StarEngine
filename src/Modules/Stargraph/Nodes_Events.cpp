#include "Nodes_Events.h"
#include "NodeRegistry.h"
#include "Core/InputKeyNames.h"
#include "Runtime/Actor.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "GraphExecutor.h"
#include "StargraphBehavior.h"
#include "ObjectPinRef.h"

void RegisterEventNodes() {
    NodeTemplate beginPlay;
    beginPlay.type = "Event_BeginPlay";
    beginPlay.category = "Events";
    beginPlay.moduleName = "Stargraph";
    beginPlay.isEvent = true;
    beginPlay.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(beginPlay);

    NodeTemplate tick;
    tick.type = "Event_Tick";
    tick.category = "Events";
    tick.moduleName = "Stargraph";
    tick.isEvent = true;
    tick.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Delta Seconds", PinType::Float, false, true});
    };
    NodeRegistry::Get().RegisterNode(tick);

    NodeTemplate endPlay;
    endPlay.type = "Event_EndPlay";
    endPlay.category = "Events";
    endPlay.moduleName = "Stargraph";
    endPlay.isEvent = true;
    endPlay.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(endPlay);

    NodeTemplate destroyed;
    destroyed.type = "Event_Destroyed";
    destroyed.category = "Events";
    destroyed.moduleName = "Stargraph";
    destroyed.isEvent = true;
    destroyed.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(destroyed);

    NodeTemplate inputKey;
    inputKey.type = "Input_Key";
    inputKey.category = "Input";
    inputKey.moduleName = "Stargraph";
    inputKey.isEvent = true;
    inputKey.createPins = [](Node* node) {
        node->inputs.push_back({"Key", PinType::String, true, false, 0, "W"});
        node->outputs.push_back({"Triggered", PinType::Exec, false, true});
        node->outputs.push_back({"Started", PinType::Exec, false, true});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
        node->outputs.push_back({"Is Held", PinType::Boolean, false, true});
        node->outputs.push_back({"Axis", PinType::Float, false, true});
        node->outputs.push_back({"Input Time", PinType::Float, false, true});
    };
    inputKey.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string keyName;
        for (const Pin& p : node.inputs) if (p.name == "Key") keyName = p.defaultValue;

        float axis = 0.0f;
        bool held = PollInputKey(keyName, axis);

        float inputTime = held ? (ctx.deltaTime * 120.0f) : 0.0f;
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%g", inputTime);
        char axisBuf[32];
        snprintf(axisBuf, sizeof(axisBuf), "%g", axis);
        for (Pin& p : node.outputs) {
            if (p.name == "Is Held")    p.defaultValue = held ? "true" : "false";
            if (p.name == "Axis")       p.defaultValue = axisBuf;
            if (p.name == "Input Time") p.defaultValue = timeBuf;
        }
    };
    NodeRegistry::Get().RegisterNode(inputKey);

    NodeTemplate customEvent;
    customEvent.type = "Custom_Event";
    customEvent.category = "Events";
    customEvent.moduleName = "Stargraph";
    customEvent.isEvent = true;
    customEvent.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(customEvent);

    NodeTemplate callCustomEvent;
    callCustomEvent.type = "Call_Custom_Event";
    callCustomEvent.category = "Events";
    callCustomEvent.moduleName = "Stargraph";
    callCustomEvent.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "Actor"});
        node->inputs.push_back({"Event Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    callCustomEvent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;

        std::string eventName;
        Actor* target = ctx.actor;
        std::vector<std::pair<std::string, std::string>> params;
        for (const Pin& p : node.inputs) {
            if (p.name == "Event Name") eventName = p.defaultValue;
            else if (p.name == "Target" && !p.defaultValue.empty()) target = ObjectPinRef::ResolveAs<Actor>(node, p.defaultValue, "Actor", ctx);
            else if (p.name != "In") params.emplace_back(p.name, p.defaultValue);
        }
        if (eventName.empty() || !target) return;

        if (target == ctx.actor) {
            GraphExecutor::RunCustomEvent(graph, eventName, ctx, params);
        } else if (GraphBehavior* behavior = target->GetGraphBehavior()) {
            behavior->OnCustomEvent(target, eventName, params);
        }

        NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
        if (backend && target->GetNetId() != 0) {
            Node* eventDef = nullptr;
            if (target == ctx.actor) {
                eventDef = graph.FindCustomEventByName(eventName);
            } else if (auto* targetBehavior = dynamic_cast<StargraphBehavior*>(target->GetGraphBehavior())) {
                eventDef = targetBehavior->GetGraph().FindCustomEventByName(eventName);
            }
            if (eventDef && eventDef->eventReplication != ReplicationMode::Local) {
                bool allowed = eventDef->eventReplication == ReplicationMode::Replicated ||
                                (eventDef->eventReplication == ReplicationMode::ServerOnly && backend->HasAuthority());
                if (allowed) {
                    ReplicatedEventCall call;
                    call.netId = target->GetNetId();
                    call.eventName = eventName;
                    call.params = params;
                    call.reliable = eventDef->eventReliable;
                    backend->SendEventCall(call, false);
                }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(callCustomEvent);
}