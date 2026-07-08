#include "Nodes_Events.h"
#include "NodeRegistry.h"
#include "InputKeyNames.h"
#include "raylib.h"
#include <cmath>

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
            if (axis > 1.0f) axis = 1.0f;
            if (axis < -1.0f) axis = -1.0f;
            held = axis != 0.0f;
        } else if (keyName == "MOUSE_Y") {
            axis = GetMouseDelta().y * -1.0f;
            if (axis > 1.0f) axis = 1.0f;
            if (axis < -1.0f) axis = -1.0f;
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
}