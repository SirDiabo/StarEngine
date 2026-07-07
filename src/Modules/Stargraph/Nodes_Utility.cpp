#include "Nodes_Utility.h"
#include "NodeRegistry.h"
#include "raylib.h"
#include <cstdlib>

static float GetScalarValue(const Pin& p) {
    if (p.type == PinType::Boolean) return (p.defaultValue == "true" || p.defaultValue == "1") ? 1.0f : 0.0f;
    if (p.type == PinType::Integer) return (float)atoi(p.defaultValue.c_str());
    return (float)atof(p.defaultValue.c_str());
}

void RegisterUtilityNodes() {
    NodeTemplate printString;
    printString.type = "Print_String";
    printString.category = "Utility";
    printString.moduleName = "Stargraph";
    printString.createPins = [](Node* node) {
        node->inputs.push_back({"In",   PinType::Exec,   true,  false});
        node->inputs.push_back({"Text", PinType::String, true,  false, 0, "Hello"});
        node->outputs.push_back({"Out", PinType::Exec,   false, true});
    };
    printString.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string text = "Hello";
        for (const Pin& pin : node.inputs) {
            if (pin.name == "Text") { text = pin.defaultValue; break; }
        }
        TraceLog(LOG_INFO, "Print_String: %s", text.c_str());
    };
    NodeRegistry::Get().RegisterNode(printString);

    NodeTemplate printWarning;
    printWarning.type = "Print_Warning";
    printWarning.category = "Utility";
    printWarning.moduleName = "Stargraph";
    printWarning.createPins = [](Node* node) {
        node->inputs.push_back({"In",   PinType::Exec,   true,  false});
        node->inputs.push_back({"Text", PinType::String, true,  false, 0, "Warning"});
        node->outputs.push_back({"Out", PinType::Exec,   false, true});
    };
    printWarning.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string text = "Warning";
        for (const Pin& pin : node.inputs) {
            if (pin.name == "Text") { text = pin.defaultValue; break; }
        }
        TraceLog(LOG_WARNING, "Print_Warning: %s", text.c_str());
    };
    NodeRegistry::Get().RegisterNode(printWarning);

    NodeTemplate printError;
    printError.type = "Print_Error";
    printError.category = "Utility";
    printError.moduleName = "Stargraph";
    printError.createPins = [](Node* node) {
        node->inputs.push_back({"In",   PinType::Exec,   true,  false});
        node->inputs.push_back({"Text", PinType::String, true,  false, 0, "Error"});
        node->outputs.push_back({"Out", PinType::Exec,   false, true});
    };
    printError.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string text = "Error";
        for (const Pin& pin : node.inputs) {
            if (pin.name == "Text") { text = pin.defaultValue; break; }
        }
        TraceLog(LOG_ERROR, "Print_Error: %s", text.c_str());
    };
    NodeRegistry::Get().RegisterNode(printError);

    static float gameTime = 0.0f;
    NodeTemplate getGameTime;
    getGameTime.type = "Get_Game_Time";
    getGameTime.category = "Utility";
    getGameTime.moduleName = "Stargraph";
    getGameTime.createPins = [](Node* node) {
        node->outputs.push_back({"Time", PinType::Float, false, true});
    };
    getGameTime.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        gameTime += ctx.deltaTime;
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", gameTime);
        for (Pin& p : node.outputs) if (p.name == "Time") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getGameTime);

    NodeTemplate getDeltaTime;
    getDeltaTime.type = "Get_Delta_Time";
    getDeltaTime.category = "Utility";
    getDeltaTime.moduleName = "Stargraph";
    getDeltaTime.createPins = [](Node* node) {
        node->outputs.push_back({"Delta Time", PinType::Float, false, true});
    };
    getDeltaTime.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", ctx.deltaTime);
        for (Pin& p : node.outputs) if (p.name == "Delta Time") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getDeltaTime);
}