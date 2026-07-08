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

    NodeTemplate strConcat;
    strConcat.type = "String_Concat";
    strConcat.category = "String";
    strConcat.moduleName = "Stargraph";
    strConcat.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"B", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    strConcat.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string a, b;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") a = p.defaultValue;
            if (p.name == "B") b = p.defaultValue;
        }
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = a + b;
    };
    NodeRegistry::Get().RegisterNode(strConcat);

    NodeTemplate strLength;
    strLength.type = "String_Length";
    strLength.category = "String";
    strLength.moduleName = "Stargraph";
    strLength.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Length", PinType::Integer, false, true});
    };
    strLength.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string in;
        for (const Pin& p : node.inputs) if (p.name == "In") in = p.defaultValue;
        for (Pin& p : node.outputs) if (p.name == "Length") p.defaultValue = std::to_string((int)in.size());
    };
    NodeRegistry::Get().RegisterNode(strLength);

    NodeTemplate strSubstring;
    strSubstring.type = "String_Substring";
    strSubstring.category = "String";
    strSubstring.moduleName = "Stargraph";
    strSubstring.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Start", PinType::Integer, true, false, 0, "0"});
        node->inputs.push_back({"Length", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    strSubstring.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string in;
        int start = 0, length = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "In") in = p.defaultValue;
            if (p.name == "Start") start = atoi(p.defaultValue.c_str());
            if (p.name == "Length") length = atoi(p.defaultValue.c_str());
        }
        std::string result;
        if (start >= 0 && start < (int)in.size() && length > 0) {
            result = in.substr(start, length);
        }
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = result;
    };
    NodeRegistry::Get().RegisterNode(strSubstring);

    NodeTemplate strFind;
    strFind.type = "String_Find";
    strFind.category = "String";
    strFind.moduleName = "Stargraph";
    strFind.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Search", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Index", PinType::Integer, false, true});
    };
    strFind.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string in, search;
        for (const Pin& p : node.inputs) {
            if (p.name == "In") in = p.defaultValue;
            if (p.name == "Search") search = p.defaultValue;
        }
        size_t pos = in.find(search);
        int result = (pos == std::string::npos) ? -1 : (int)pos;
        for (Pin& p : node.outputs) if (p.name == "Index") p.defaultValue = std::to_string(result);
    };
    NodeRegistry::Get().RegisterNode(strFind);

    NodeTemplate strReplace;
    strReplace.type = "String_Replace";
    strReplace.category = "String";
    strReplace.moduleName = "Stargraph";
    strReplace.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Find", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Replace_With", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    strReplace.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string in, find, replaceWith;
        for (const Pin& p : node.inputs) {
            if (p.name == "In") in = p.defaultValue;
            if (p.name == "Find") find = p.defaultValue;
            if (p.name == "Replace_With") replaceWith = p.defaultValue;
        }
        std::string result = in;
        if (!find.empty()) {
            size_t pos = 0;
            while ((pos = result.find(find, pos)) != std::string::npos) {
                result.replace(pos, find.size(), replaceWith);
                pos += replaceWith.size();
            }
        }
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = result;
    };
    NodeRegistry::Get().RegisterNode(strReplace);

    NodeTemplate strContains;
    strContains.type = "String_Contains";
    strContains.category = "String";
    strContains.moduleName = "Stargraph";
    strContains.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Search", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Result", PinType::Boolean, false, true});
    };
    strContains.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string in, search;
        for (const Pin& p : node.inputs) {
            if (p.name == "In") in = p.defaultValue;
            if (p.name == "Search") search = p.defaultValue;
        }
        bool result = in.find(search) != std::string::npos;
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = result ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(strContains);
}