#include "Nodes_Events.h"
#include "NodeRegistry.h"
#include "InputKeyNames.h"
#include "raylib.h"

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

    NodeTemplate getAxisH;
    getAxisH.type = "Get_Axis_Horizontal";
    getAxisH.category = "Input";
    getAxisH.moduleName = "Stargraph";
    getAxisH.createPins = [](Node* node) {
        node->outputs.push_back({"Axis", PinType::Float, false, true});
    };
    getAxisH.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float axis = 0;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) axis += 1.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  axis -= 1.0f;
        char buf[32]; snprintf(buf, sizeof(buf), "%g", axis);
        for (Pin& p : node.outputs) if (p.name == "Axis") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getAxisH);

    NodeTemplate getAxisV;
    getAxisV.type = "Get_Axis_Vertical";
    getAxisV.category = "Input";
    getAxisV.moduleName = "Stargraph";
    getAxisV.createPins = [](Node* node) {
        node->outputs.push_back({"Axis", PinType::Float, false, true});
    };
    getAxisV.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float axis = 0;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   axis += 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  axis -= 1.0f;
        char buf[32]; snprintf(buf, sizeof(buf), "%g", axis);
        for (Pin& p : node.outputs) if (p.name == "Axis") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getAxisV);
}