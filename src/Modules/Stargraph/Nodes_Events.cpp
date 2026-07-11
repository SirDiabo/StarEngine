#include "Nodes_Events.h"
#include "NodeRegistry.h"
#include "InputKeyNames.h"
#include "Runtime/Actor.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "GraphExecutor.h"
#include "StargraphBehavior.h"
#include "raylib.h"
#include <cmath>
#include <cstdlib>

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

        bool held = false;
        float axis = 0.0f;

        if (keyName == "LEFT_MOUSE") {
            held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        } else if (keyName == "RIGHT_MOUSE") {
            held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        } else if (keyName == "MIDDLE_MOUSE") {
            held = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        } else if (keyName == "MOUSE_WHEEL") {
            axis = GetMouseWheelMove();
            if (axis > 1.0f) axis = 1.0f;
            if (axis < -1.0f) axis = -1.0f;
            held = axis != 0.0f;
        } else if (keyName == "MOUSE_X") {
            axis = GetMouseDelta().x * -1.0f;
            held = axis != 0.0f;
        } else if (keyName == "MOUSE_Y") {
            axis = GetMouseDelta().y * -1.0f;
            held = axis != 0.0f;
        } else if (keyName.rfind("GAMEPAD_", 0) == 0) {
            const int pad = 0;
            bool avail = IsGamepadAvailable(pad);
            if (keyName == "GAMEPAD_FACE_UP")         held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP);
            else if (keyName == "GAMEPAD_FACE_DOWN")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
            else if (keyName == "GAMEPAD_FACE_LEFT")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
            else if (keyName == "GAMEPAD_FACE_RIGHT") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
            else if (keyName == "GAMEPAD_DPAD_UP")    held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_UP);
            else if (keyName == "GAMEPAD_DPAD_DOWN")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
            else if (keyName == "GAMEPAD_DPAD_LEFT")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
            else if (keyName == "GAMEPAD_DPAD_RIGHT") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
            else if (keyName == "GAMEPAD_SHOULDER_L") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
            else if (keyName == "GAMEPAD_SHOULDER_R") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
            else if (keyName == "GAMEPAD_SPECIAL_L")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_MIDDLE_LEFT);
            else if (keyName == "GAMEPAD_SPECIAL_R")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_MIDDLE_RIGHT);
            else if (keyName == "GAMEPAD_LEFT_THUMB")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_THUMB);
            else if (keyName == "GAMEPAD_RIGHT_THUMB") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_THUMB);
            else if (keyName == "GAMEPAD_LEFT_STICK_X")  { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X)  : 0.0f; held = fabsf(axis) > 0.1f; }
            else if (keyName == "GAMEPAD_LEFT_STICK_Y")  { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y)  : 0.0f; held = fabsf(axis) > 0.1f; }
            else if (keyName == "GAMEPAD_RIGHT_STICK_X") { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_X) : 0.0f; held = fabsf(axis) > 0.1f; }
            else if (keyName == "GAMEPAD_RIGHT_STICK_Y") { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_Y) : 0.0f; held = fabsf(axis) > 0.1f; }
            else if (keyName == "GAMEPAD_TRIGGER_L") { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_TRIGGER)  : 0.0f; held = axis > 0.1f; }
            else if (keyName == "GAMEPAD_TRIGGER_R") { axis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_TRIGGER) : 0.0f; held = axis > 0.1f; }
        } else {
            int key = InputKeyNameToCode(keyName);
            held = key != 0 && IsKeyDown(key);
        }

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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
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
            else if (p.name == "Target" && !p.defaultValue.empty()) target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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