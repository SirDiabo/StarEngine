#include "Nodes_Utility.h"
#include "NodeRegistry.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/CameraComponent.h"
#include "raylib.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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

    NodeTemplate getScreenSize;
    getScreenSize.type = "Get_Screen_Size";
    getScreenSize.category = "Utility";
    getScreenSize.moduleName = "Stargraph";
    getScreenSize.createPins = [](Node* node) {
        node->outputs.push_back({"Width", PinType::Integer, false, true});
        node->outputs.push_back({"Height", PinType::Integer, false, true});
    };
    getScreenSize.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (Pin& p : node.outputs) {
            if (p.name == "Width") p.defaultValue = std::to_string(GetScreenWidth());
            if (p.name == "Height") p.defaultValue = std::to_string(GetScreenHeight());
        }
    };
    NodeRegistry::Get().RegisterNode(getScreenSize);

    NodeTemplate getMousePos;
    getMousePos.type = "Get_Mouse_Position";
    getMousePos.category = "Utility";
    getMousePos.moduleName = "Stargraph";
    getMousePos.createPins = [](Node* node) {
        node->outputs.push_back({"X", PinType::Float, false, true});
        node->outputs.push_back({"Y", PinType::Float, false, true});
    };
    getMousePos.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector2 mp = GetMousePosition();
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", mp.x);
        for (Pin& p : node.outputs) if (p.name == "X") p.defaultValue = buf;
        snprintf(buf, sizeof(buf), "%g", mp.y);
        for (Pin& p : node.outputs) if (p.name == "Y") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getMousePos);

    NodeTemplate getMouseDelta;
    getMouseDelta.type = "Get_Mouse_Delta";
    getMouseDelta.category = "Input";
    getMouseDelta.moduleName = "Stargraph";
    getMouseDelta.createPins = [](Node* node) {
        node->outputs.push_back({"X", PinType::Float, false, true});
        node->outputs.push_back({"Y", PinType::Float, false, true});
    };
    getMouseDelta.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector2 delta = GetMouseDelta();
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", delta.x);
        for (Pin& p : node.outputs) if (p.name == "X") p.defaultValue = buf;
        snprintf(buf, sizeof(buf), "%g", delta.y);
        for (Pin& p : node.outputs) if (p.name == "Y") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getMouseDelta);

    NodeTemplate getFPS;
    getFPS.type = "Get_FPS";
    getFPS.category = "Utility";
    getFPS.moduleName = "Stargraph";
    getFPS.createPins = [](Node* node) {
        node->outputs.push_back({"FPS", PinType::Integer, false, true});
    };
    getFPS.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (Pin& p : node.outputs) if (p.name == "FPS") p.defaultValue = std::to_string(GetFPS());
    };
    NodeRegistry::Get().RegisterNode(getFPS);

    NodeTemplate setWindowTitle;
    setWindowTitle.type = "Set_Window_Title";
    setWindowTitle.category = "Utility";
    setWindowTitle.moduleName = "Stargraph";
    setWindowTitle.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Title", PinType::String, true, false, 0, "StarEngine"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setWindowTitle.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string title = "StarEngine";
        for (const Pin& p : node.inputs) {
            if (p.name == "Title") title = p.defaultValue;
        }
        SetWindowTitle(title.c_str());
    };
    NodeRegistry::Get().RegisterNode(setWindowTitle);

    NodeTemplate toggleFullscreen;
    toggleFullscreen.type = "Toggle_Fullscreen";
    toggleFullscreen.category = "Window";
    toggleFullscreen.moduleName = "Stargraph";
    toggleFullscreen.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    toggleFullscreen.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        ToggleFullscreen();
    };
    NodeRegistry::Get().RegisterNode(toggleFullscreen);

    NodeTemplate maximizeWindow;
    maximizeWindow.type = "Maximize_Window";
    maximizeWindow.category = "Window";
    maximizeWindow.moduleName = "Stargraph";
    maximizeWindow.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    maximizeWindow.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        MaximizeWindow();
    };
    NodeRegistry::Get().RegisterNode(maximizeWindow);

    NodeTemplate minimizeWindow;
    minimizeWindow.type = "Minimize_Window";
    minimizeWindow.category = "Window";
    minimizeWindow.moduleName = "Stargraph";
    minimizeWindow.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    minimizeWindow.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        MinimizeWindow();
    };
    NodeRegistry::Get().RegisterNode(minimizeWindow);

    NodeTemplate colorFromHex;
    colorFromHex.type = "Color_From_Hex";
    colorFromHex.category = "Color";
    colorFromHex.moduleName = "Stargraph";
    colorFromHex.createPins = [](Node* node) {
        node->inputs.push_back({"Hex", PinType::String, true, false, 0, "FFFFFF"});
        node->outputs.push_back({"Color", PinType::Vec3, false, true});
    };
    colorFromHex.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string hex = "FFFFFF";
        for (const Pin& p : node.inputs) {
            if (p.name == "Hex") hex = p.defaultValue;
        }
        for (char& c : hex) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        }
        unsigned int r = 0, g = 0, b = 0;
        if (hex.length() == 6) {
            sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b);
        } else if (hex.length() == 8) {
            unsigned int a = 0;
            sscanf(hex.c_str(), "%02x%02x%02x%02x", &r, &g, &b, &a);
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%g,%g,%g", (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f);
        for (Pin& p : node.outputs) if (p.name == "Color") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(colorFromHex);

    NodeTemplate colorToHex;
    colorToHex.type = "Color_To_Hex";
    colorToHex.category = "Color";
    colorToHex.moduleName = "Stargraph";
    colorToHex.createPins = [](Node* node) {
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Hex", PinType::String, false, true});
    };
    colorToHex.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float r = 1, g = 1, b = 1;
        for (const Pin& p : node.inputs) {
            if (p.name == "Color") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
        }
        int ri = (int)(r * 255), gi = (int)(g * 255), bi = (int)(b * 255);
        if (ri < 0) ri = 0; if (ri > 255) ri = 255;
        if (gi < 0) gi = 0; if (gi > 255) gi = 255;
        if (bi < 0) bi = 0; if (bi > 255) bi = 255;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02X%02X%02X", ri, gi, bi);
        for (Pin& p : node.outputs) if (p.name == "Hex") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(colorToHex);

    NodeTemplate colorLerp;
    colorLerp.type = "Color_Lerp";
    colorLerp.category = "Color";
    colorLerp.moduleName = "Stargraph";
    colorLerp.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "1,1,1"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "1,1,1"});
        node->inputs.push_back({"Alpha", PinType::Float, true, false, 0, "0.5"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    colorLerp.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float ra = 1, ga = 1, ba = 1;
        float rb = 1, gb = 1, bb = 1;
        float alpha = 0.5f;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &ra, &ga, &ba);
            if (p.name == "B") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &rb, &gb, &bb);
            if (p.name == "Alpha") alpha = (float)atof(p.defaultValue.c_str());
        }
        float r = ra + (rb - ra) * alpha;
        float g = ga + (gb - ga) * alpha;
        float b = ba + (bb - ba) * alpha;
        char buf[64];
        snprintf(buf, sizeof(buf), "%g,%g,%g", r, g, b);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(colorLerp);

    NodeTemplate colorTint;
    colorTint.type = "Color_Tint";
    colorTint.category = "Color";
    colorTint.moduleName = "Stargraph";
    colorTint.createPins = [](Node* node) {
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->inputs.push_back({"Tint", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    colorTint.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float cr = 1, cg = 1, cb = 1;
        float tr = 1, tg = 1, tb = 1;
        for (const Pin& p : node.inputs) {
            if (p.name == "Color") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &cr, &cg, &cb);
            if (p.name == "Tint") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &tr, &tg, &tb);
        }
        float r = cr * tr;
        float g = cg * tg;
        float b = cb * tb;
        char buf[64];
        snprintf(buf, sizeof(buf), "%g,%g,%g", r, g, b);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(colorTint);

    NodeTemplate colorAlpha;
    colorAlpha.type = "Color_Alpha";
    colorAlpha.category = "Color";
    colorAlpha.moduleName = "Stargraph";
    colorAlpha.createPins = [](Node* node) {
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->inputs.push_back({"Alpha", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    colorAlpha.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float r = 1, g = 1, b = 1;
        float alpha = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Color") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
            if (p.name == "Alpha") alpha = (float)atof(p.defaultValue.c_str());
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%g,%g,%g,%g", r, g, b, alpha);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(colorAlpha);

    NodeTemplate drawGrid;
    drawGrid.type = "Draw_Grid";
    drawGrid.category = "Debug";
    drawGrid.moduleName = "Stargraph";
    drawGrid.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Center", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Float, true, false, 0, "100.0"});
        node->inputs.push_back({"Lines", PinType::Integer, true, false, 0, "10"});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawGrid.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 center = {0, 0, 0};
        float size = 100.0f;
        int lines = 10;
        Color color = WHITE;
        for (const Pin& p : node.inputs) {
            if (p.name == "Center") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &center.x, &center.y, &center.z);
            if (p.name == "Size") size = (float)atof(p.defaultValue.c_str());
            if (p.name == "Lines") lines = atoi(p.defaultValue.c_str());
            if (p.name == "Color") {
                float r = 1, g = 1, b = 1;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        float step = size / (float)(lines - 1);
        for (int i = 0; i < lines; i++) {
            float pos = -size * 0.5f + step * i;
            DrawLine3D({center.x + pos, center.y, center.z + size * 0.5f},
                       {center.x + pos, center.y, center.z - size * 0.5f}, color);
            DrawLine3D({center.x + size * 0.5f, center.y, center.z + pos},
                       {center.x - size * 0.5f, center.y, center.z + pos}, color);
        }
    };
    NodeRegistry::Get().RegisterNode(drawGrid);

    NodeTemplate drawAxes;
    drawAxes.type = "Draw_Axes";
    drawAxes.category = "Debug";
    drawAxes.moduleName = "Stargraph";
    drawAxes.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Float, true, false, 0, "50.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawAxes.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 pos = {0, 0, 0};
        float size = 50.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Size") size = (float)atof(p.defaultValue.c_str());
        }
        DrawLine3D(pos, {pos.x + size, pos.y, pos.z}, RED);
        DrawLine3D(pos, {pos.x, pos.y + size, pos.z}, GREEN);
        DrawLine3D(pos, {pos.x, pos.y, pos.z + size}, BLUE);
    };
    NodeRegistry::Get().RegisterNode(drawAxes);

    NodeTemplate drawCube;
    drawCube.type = "Draw_Cube";
    drawCube.category = "Debug";
    drawCube.moduleName = "Stargraph";
    drawCube.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Vec3, true, false, 0, "10,10,10"});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawCube.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 pos = {0, 0, 0};
        Vector3 size = {10, 10, 10};
        Color color = RED;
        for (const Pin& p : node.inputs) {
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Size") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &size.x, &size.y, &size.z);
            if (p.name == "Color") {
                float r = 1, g = 0, b = 0;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        DrawCube(pos, size.x, size.y, size.z, color);
    };
    NodeRegistry::Get().RegisterNode(drawCube);

    NodeTemplate drawCircle3D;
    drawCircle3D.type = "Draw_Circle_3D";
    drawCircle3D.category = "Debug";
    drawCircle3D.moduleName = "Stargraph";
    drawCircle3D.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Radius", PinType::Float, true, false, 0, "50.0"});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawCircle3D.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 pos = {0, 0, 0};
        float radius = 50.0f;
        Color color = WHITE;
        for (const Pin& p : node.inputs) {
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Radius") radius = (float)atof(p.defaultValue.c_str());
            if (p.name == "Color") {
                float r = 1, g = 1, b = 1;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        DrawSphere(pos, radius, color);
        DrawSphereWires(pos, radius, 8, 12, {0, 0, 0, 255});
    };
    NodeRegistry::Get().RegisterNode(drawCircle3D);

    NodeTemplate drawRay;
    drawRay.type = "Draw_Ray";
    drawRay.category = "Debug";
    drawRay.moduleName = "Stargraph";
    drawRay.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Start", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"End", PinType::Vec3, true, false, 0, "0,0,100"});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawRay.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 start = {0, 0, 0};
        Vector3 end = {0, 0, 100};
        Color color = WHITE;
        for (const Pin& p : node.inputs) {
            if (p.name == "Start") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &start.x, &start.y, &start.z);
            if (p.name == "End") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &end.x, &end.y, &end.z);
            if (p.name == "Color") {
                float r = 1, g = 1, b = 1;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        DrawLine3D(start, end, color);
    };
    NodeRegistry::Get().RegisterNode(drawRay);

    NodeTemplate fileExists;
    fileExists.type = "File_Exists";
    fileExists.category = "File I/O";
    fileExists.moduleName = "Stargraph";
    fileExists.createPins = [](Node* node) {
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Exists", PinType::Boolean, false, true});
    };
    fileExists.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
        }
        bool exists = fs::exists(path);
        for (Pin& p : node.outputs) if (p.name == "Exists") p.defaultValue = exists ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(fileExists);

    NodeTemplate dirExists;
    dirExists.type = "Directory_Exists";
    dirExists.category = "File I/O";
    dirExists.moduleName = "Stargraph";
    dirExists.createPins = [](Node* node) {
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Exists", PinType::Boolean, false, true});
    };
    dirExists.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
        }
        bool exists = fs::is_directory(path);
        for (Pin& p : node.outputs) if (p.name == "Exists") p.defaultValue = exists ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(dirExists);

    NodeTemplate loadTextFile;
    loadTextFile.type = "Load_Text_File";
    loadTextFile.category = "File I/O";
    loadTextFile.moduleName = "Stargraph";
    loadTextFile.createPins = [](Node* node) {
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Content", PinType::String, false, true});
    };
    loadTextFile.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
        }
        std::ifstream in(path);
        std::string content;
        if (in.is_open()) {
            content = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        for (Pin& p : node.outputs) if (p.name == "Content") p.defaultValue = content;
    };
    NodeRegistry::Get().RegisterNode(loadTextFile);

    NodeTemplate saveTextFile;
    saveTextFile.type = "Save_Text_File";
    saveTextFile.category = "File I/O";
    saveTextFile.moduleName = "Stargraph";
    saveTextFile.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Content", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveTextFile.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path, content;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
            if (p.name == "Content") content = p.defaultValue;
        }
        std::ofstream out(path);
        if (out.is_open()) {
            out << content;
        }
    };
    NodeRegistry::Get().RegisterNode(saveTextFile);

    NodeTemplate getWorkingDir;
    getWorkingDir.type = "Get_Working_Directory";
    getWorkingDir.category = "File I/O";
    getWorkingDir.moduleName = "Stargraph";
    getWorkingDir.createPins = [](Node* node) {
        node->outputs.push_back({"Path", PinType::String, false, true});
    };
    getWorkingDir.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (Pin& p : node.outputs) if (p.name == "Path") p.defaultValue = fs::current_path().string();
    };
    NodeRegistry::Get().RegisterNode(getWorkingDir);

    NodeTemplate changeDir;
    changeDir.type = "Change_Directory";
    changeDir.category = "File I/O";
    changeDir.moduleName = "Stargraph";
    changeDir.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    changeDir.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
        }
        if (!path.empty()) {
            fs::current_path(path);
        }
    };
    NodeRegistry::Get().RegisterNode(changeDir);

    NodeTemplate drawPrimitive;
    drawPrimitive.type = "Draw_Primitive";
    drawPrimitive.category = "Drawing";
    drawPrimitive.moduleName = "Stargraph";
    drawPrimitive.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Type", PinType::String, true, false, 0, "Rectangle"});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Vec3, true, false, 0, "100,100,0"});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawPrimitive.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string type = "Rectangle";
        Vector3 pos = {0, 0, 0};
        Vector3 size = {100, 100, 0};
        Color color = WHITE;
        for (const Pin& p : node.inputs) {
            if (p.name == "Type") type = p.defaultValue;
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Size") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &size.x, &size.y, &size.z);
            if (p.name == "Color") {
                float r = 1, g = 1, b = 1;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        if (type == "Rectangle") DrawRectangle((int)pos.x, (int)pos.y, (int)size.x, (int)size.y, color);
        else if (type == "Circle") DrawCircle((int)pos.x, (int)pos.y, size.x, color);
        else if (type == "Line") DrawLine((int)pos.x, (int)pos.y, (int)(pos.x + size.x), (int)(pos.y + size.y), color);
        else if (type == "Triangle") {
            Vector2 v1 = {pos.x, pos.y};
            Vector2 v2 = {pos.x + size.x, pos.y};
            Vector2 v3 = {pos.x + size.x * 0.5f, pos.y + size.y};
            DrawTriangle(v1, v2, v3, color);
        }
    };
    NodeRegistry::Get().RegisterNode(drawPrimitive);

    NodeTemplate clearBackground;
    clearBackground.type = "Clear_Background";
    clearBackground.category = "Drawing";
    clearBackground.moduleName = "Stargraph";
    clearBackground.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Color", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    clearBackground.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Color color = BLACK;
        for (const Pin& p : node.inputs) {
            if (p.name == "Color") {
                float r = 0, g = 0, b = 0;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                color = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        ClearBackground(color);
    };
    NodeRegistry::Get().RegisterNode(clearBackground);

    NodeTemplate drawBillboard;
    drawBillboard.type = "Draw_Billboard";
    drawBillboard.category = "Drawing";
    drawBillboard.moduleName = "Stargraph";
    drawBillboard.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Float, true, false, 0, "100.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawBillboard.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 pos = {0, 0, 0};
        float size = 100.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Size") size = (float)atof(p.defaultValue.c_str());
        }
        Camera3D cam = {0};
        cam.up = {0, 1, 0};
        Actor* viewTarget = World::Get().GetViewTarget();
        if (viewTarget && viewTarget->GetCameraComponent()) {
            CameraComponent* cc = viewTarget->GetCameraComponent();
            Vector3 compPos = Actor::ComputeWorldPosition(cc->position, viewTarget->position, viewTarget->rotation, viewTarget->scale);
            Vector3 compRot = Actor::ComputeWorldRotation(cc->rotation, viewTarget->rotation);
            float yr = compRot.y * DEG2RAD;
            float pr = compRot.x * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            cam.position = compPos;
            cam.target = {compPos.x + forward.x, compPos.y + forward.y, compPos.z + forward.z};
            cam.fovy = cc->fov;
            cam.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
        } else if (ctx.actor) {
            Vector3 forward = {cosf(ctx.actor->rotation.y * DEG2RAD), 0, sinf(ctx.actor->rotation.y * DEG2RAD)};
            cam.position = ctx.actor->position;
            cam.target = {ctx.actor->position.x + forward.x, ctx.actor->position.y + forward.y, ctx.actor->position.z + forward.z};
            cam.fovy = 50.0f;
            cam.projection = CAMERA_PERSPECTIVE;
        }
        DrawBillboard(cam, Texture2D{}, pos, size, WHITE);
    };
    NodeRegistry::Get().RegisterNode(drawBillboard);

    NodeTemplate drawBillboardPro;
    drawBillboardPro.type = "Draw_Billboard_Pro";
    drawBillboardPro.category = "Drawing";
    drawBillboardPro.moduleName = "Stargraph";
    drawBillboardPro.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Position", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Size", PinType::Vec3, true, false, 0, "100,100,0"});
        node->inputs.push_back({"Rotation", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Tint", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    drawBillboardPro.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 pos = {0, 0, 0};
        Vector3 size = {100, 100, 0};
        float rot = 0.0f;
        Color tint = WHITE;
        for (const Pin& p : node.inputs) {
            if (p.name == "Position") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
            if (p.name == "Size") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &size.x, &size.y, &size.z);
            if (p.name == "Rotation") rot = (float)atof(p.defaultValue.c_str());
            if (p.name == "Tint") {
                float r = 1, g = 1, b = 1;
                sscanf(p.defaultValue.c_str(), "%f,%f,%f", &r, &g, &b);
                tint = { (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
            }
        }
        Camera3D cam = {0};
        cam.up = {0, 1, 0};
        Actor* viewTarget = World::Get().GetViewTarget();
        if (viewTarget && viewTarget->GetCameraComponent()) {
            CameraComponent* cc = viewTarget->GetCameraComponent();
            Vector3 compPos = Actor::ComputeWorldPosition(cc->position, viewTarget->position, viewTarget->rotation, viewTarget->scale);
            Vector3 compRot = Actor::ComputeWorldRotation(cc->rotation, viewTarget->rotation);
            float yr = compRot.y * DEG2RAD;
            float pr = compRot.x * DEG2RAD;
            Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
            cam.position = compPos;
            cam.target = {compPos.x + forward.x, compPos.y + forward.y, compPos.z + forward.z};
            cam.fovy = cc->fov;
            cam.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
        } else if (ctx.actor) {
            Vector3 forward = {cosf(ctx.actor->rotation.y * DEG2RAD), 0, sinf(ctx.actor->rotation.y * DEG2RAD)};
            cam.position = ctx.actor->position;
            cam.target = {ctx.actor->position.x + forward.x, ctx.actor->position.y + forward.y, ctx.actor->position.z + forward.z};
            cam.fovy = 50.0f;
            cam.projection = CAMERA_PERSPECTIVE;
        }
        Rectangle rec = {0, 0, 0, 0};
        Vector2 billboardSize = {size.x, size.y};
        Vector2 origin = {billboardSize.x * 0.5f, billboardSize.y * 0.5f};
        DrawBillboardPro(cam, Texture2D{}, rec, pos, {0, 1, 0}, billboardSize, origin, rot, tint);
    };
    NodeRegistry::Get().RegisterNode(drawBillboardPro);
}