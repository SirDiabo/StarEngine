#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "raylib.h"

StargraphModule::StargraphModule() {
    name = "Stargraph";
}

bool StargraphModule::Initialize() {
    RegisterDefaultNodes();
    return true;
}

void StargraphModule::Shutdown() {
}

void StargraphModule::Update(float deltaTime) {
}

void StargraphModule::RegisterDefaultNodes() {
    NodeTemplate beginPlay;
    beginPlay.type = "Event_BeginPlay";
    beginPlay.category = "Events";
    beginPlay.isEvent = true;
    beginPlay.createPins = [](Node* node) {
        Pin out;
        out.name = "Out";
        out.type = PinType::Exec;
        out.isOutput = true;
        node->outputs.push_back(out);
    };
    NodeRegistry::Get().RegisterNode(beginPlay);

    NodeTemplate tick;
    tick.type = "Event_Tick";
    tick.category = "Events";
    tick.isEvent = true;
    tick.createPins = [](Node* node) {
        Pin out;
        out.name = "Out";
        out.type = PinType::Exec;
        out.isOutput = true;
        node->outputs.push_back(out);

        Pin delta;
        delta.name = "Delta Seconds";
        delta.type = PinType::Float;
        delta.isOutput = true;
        node->outputs.push_back(delta);
    };
    NodeRegistry::Get().RegisterNode(tick);

    NodeTemplate endPlay;
    endPlay.type = "Event_EndPlay";
    endPlay.category = "Events";
    endPlay.isEvent = true;
    endPlay.createPins = [](Node* node) {
        Pin out;
        out.name = "Out";
        out.type = PinType::Exec;
        out.isOutput = true;
        node->outputs.push_back(out);
    };
    NodeRegistry::Get().RegisterNode(endPlay);

    NodeTemplate printString;
    printString.type = "Print_String";
    printString.category = "Utility";
    printString.createPins = [](Node* node) {
        Pin in;
        in.name = "In";
        in.type = PinType::Exec;
        in.isInput = true;
        node->inputs.push_back(in);

        Pin text;
        text.name = "Text";
        text.type = PinType::String;
        text.isInput = true;
        text.defaultValue = "Hello";
        node->inputs.push_back(text);

        Pin out;
        out.name = "Out";
        out.type = PinType::Exec;
        out.isOutput = true;
        node->outputs.push_back(out);
    };
    printString.execute = [](Node& node) {
        std::string text = "Hello";
        for (const Pin& pin : node.inputs) {
            if (pin.name == "Text") {
                text = pin.defaultValue;
                break;
            }
        }
        TraceLog(LOG_INFO, "Print_String: %s", text.c_str());
    };
    NodeRegistry::Get().RegisterNode(printString);
}
