#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "Core/AssetTypeRegistry.h"
#include "raylib.h"
#include "raymath.h"
#include <cstdlib>

StargraphModule::StargraphModule() {
    name = "Stargraph";
}

bool StargraphModule::Initialize() {
    RegisterDefaultNodes();
    RegisterAssetType();
    return true;
}

void StargraphModule::Shutdown() {}
void StargraphModule::Update(float deltaTime) {}

static void RegisterFloatMath(const std::string& nodeType, char op) {
    NodeTemplate t;
    t.type = nodeType;
    t.category = "Math";
    t.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"B", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    switch (op) {
        case '+': t.execute = [](Node& node, Stargraph&) {
            float a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = (float)atof(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = (float)atof(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a + b);
        }; break;
        case '-': t.execute = [](Node& node, Stargraph&) {
            float a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = (float)atof(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = (float)atof(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a - b);
        }; break;
        case '*': t.execute = [](Node& node, Stargraph&) {
            float a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = (float)atof(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = (float)atof(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a * b);
        }; break;
        case '/': t.execute = [](Node& node, Stargraph&) {
            float a = 0, b = 1;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = (float)atof(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = (float)atof(p.defaultValue.c_str());
            }
            float result = (b != 0.0f) ? a / b : 0.0f;
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(result);
        }; break;
    }
    NodeRegistry::Get().RegisterNode(t);
}

static void RegisterIntMath(const std::string& nodeType, char op) {
    NodeTemplate t;
    t.type = nodeType;
    t.category = "Math";
    t.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Integer, true, false, 0, "0"});
        node->inputs.push_back({"B", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::Integer, false, true});
    };
    switch (op) {
        case '+': t.execute = [](Node& node, Stargraph&) {
            int a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = atoi(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = atoi(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a + b);
        }; break;
        case '-': t.execute = [](Node& node, Stargraph&) {
            int a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = atoi(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = atoi(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a - b);
        }; break;
        case '*': t.execute = [](Node& node, Stargraph&) {
            int a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = atoi(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = atoi(p.defaultValue.c_str());
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(a * b);
        }; break;
        case '/': t.execute = [](Node& node, Stargraph&) {
            int a = 0, b = 1;
            for (const Pin& p : node.inputs) {
                if (p.name == "A" && !p.defaultValue.empty()) a = atoi(p.defaultValue.c_str());
                if (p.name == "B" && !p.defaultValue.empty()) b = atoi(p.defaultValue.c_str());
            }
            int result = (b != 0) ? a / b : 0;
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(result);
        }; break;
    }
    NodeRegistry::Get().RegisterNode(t);
}

static void RegisterVec3Math(const std::string& nodeType, char op) {
    NodeTemplate t;
    t.type = nodeType;
    t.category = "Math";
    t.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    auto parse = [](const std::string& s) -> Vector3 {
        Vector3 v = {0, 0, 0};
        sscanf(s.c_str(), "%f,%f,%f", &v.x, &v.y, &v.z);
        return v;
    };
    switch (op) {
        case '+': t.execute = [](Node& node, Stargraph&) {
            Vector3 a = {0,0,0}, b = {0,0,0};
            for (const Pin& p : node.inputs) {
                if (p.name == "A") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &a.x, &a.y, &a.z);
                if (p.name == "B") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &b.x, &b.y, &b.z);
            }
            Vector3 r = Vector3Add(a, b);
            char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
        }; break;
        case '-': t.execute = [](Node& node, Stargraph&) {
            Vector3 a = {0,0,0}, b = {0,0,0};
            for (const Pin& p : node.inputs) {
                if (p.name == "A") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &a.x, &a.y, &a.z);
                if (p.name == "B") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &b.x, &b.y, &b.z);
            }
            Vector3 r = Vector3Subtract(a, b);
            char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
        }; break;
        case '*': t.execute = [](Node& node, Stargraph&) {
            Vector3 a = {0,0,0}, b = {0,0,0};
            for (const Pin& p : node.inputs) {
                if (p.name == "A") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &a.x, &a.y, &a.z);
                if (p.name == "B") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &b.x, &b.y, &b.z);
            }
            Vector3 r = Vector3Multiply(a, b);
            char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
        }; break;
        case '/': t.execute = [](Node& node, Stargraph&) {
            Vector3 a = {0,0,0}, b = {1,1,1};
            for (const Pin& p : node.inputs) {
                if (p.name == "A") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &a.x, &a.y, &a.z);
                if (p.name == "B") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &b.x, &b.y, &b.z);
            }
            Vector3 r = Vector3Divide(a, b);
            char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
        }; break;
    }
    NodeRegistry::Get().RegisterNode(t);
}

void StargraphModule::RegisterDefaultNodes() {

    NodeTemplate beginPlay;
    beginPlay.type = "Event_BeginPlay";
    beginPlay.category = "Events";
    beginPlay.isEvent = true;
    beginPlay.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(beginPlay);

    NodeTemplate tick;
    tick.type = "Event_Tick";
    tick.category = "Events";
    tick.isEvent = true;
    tick.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Delta Seconds", PinType::Float, false, true});
    };
    NodeRegistry::Get().RegisterNode(tick);

    NodeTemplate endPlay;
    endPlay.type = "Event_EndPlay";
    endPlay.category = "Events";
    endPlay.isEvent = true;
    endPlay.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(endPlay);

    NodeTemplate printString;
    printString.type = "Print_String";
    printString.category = "Utility";
    printString.createPins = [](Node* node) {
        node->inputs.push_back({"In",   PinType::Exec,   true,  false});
        node->inputs.push_back({"Text", PinType::String, true,  false, 0, "Hello"});
        node->outputs.push_back({"Out", PinType::Exec,   false, true});
    };
    printString.execute = [](Node& node, Stargraph&) {
        std::string text = "Hello";
        for (const Pin& pin : node.inputs) {
            if (pin.name == "Text") { text = pin.defaultValue; break; }
        }
        TraceLog(LOG_INFO, "Print_String: %s", text.c_str());
    };
    NodeRegistry::Get().RegisterNode(printString);

    NodeTemplate branch;
    branch.type = "Branch";
    branch.category = "Flow";
    branch.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"True",  PinType::Exec, false, true});
        node->outputs.push_back({"False", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(branch);

    NodeTemplate forLoop;
    forLoop.type = "For_Loop";
    forLoop.category = "Flow";
    forLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec,    true,  false});
        node->inputs.push_back({"First", PinType::Integer, true,  false, 0, "0"});
        node->inputs.push_back({"Last",  PinType::Integer, true,  false, 0, "9"});
        node->outputs.push_back({"Loop Body", PinType::Exec,    false, true});
        node->outputs.push_back({"Index",     PinType::Integer, false, true});
        node->outputs.push_back({"Completed", PinType::Exec,    false, true});
    };
    NodeRegistry::Get().RegisterNode(forLoop);

    NodeTemplate whileLoop;
    whileLoop.type = "While_Loop";
    whileLoop.category = "Flow";
    whileLoop.createPins = [](Node* node) {
        node->inputs.push_back({"In",        PinType::Exec,    true,  false});
        node->inputs.push_back({"Condition", PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"Loop Body", PinType::Exec, false, true});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(whileLoop);

    NodeTemplate sequence;
    sequence.type = "Sequence";
    sequence.category = "Flow";
    sequence.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"0", PinType::Exec, false, true});
        node->outputs.push_back({"1", PinType::Exec, false, true});
        node->outputs.push_back({"2", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(sequence);

    RegisterFloatMath("Float_Add",      '+');
    RegisterFloatMath("Float_Subtract", '-');
    RegisterFloatMath("Float_Multiply", '*');
    RegisterFloatMath("Float_Divide",   '/');

    RegisterIntMath("Int_Add",      '+');
    RegisterIntMath("Int_Subtract", '-');
    RegisterIntMath("Int_Multiply", '*');
    RegisterIntMath("Int_Divide",   '/');

    RegisterVec3Math("Vec3_Add",      '+');
    RegisterVec3Math("Vec3_Subtract", '-');
    RegisterVec3Math("Vec3_Multiply", '*');
    RegisterVec3Math("Vec3_Divide",   '/');

    NodeTemplate floatToStr;
    floatToStr.type = "Float_To_String";
    floatToStr.category = "Conversions";
    floatToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    floatToStr.execute = [](Node& node, Stargraph&) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = (float)atof(p.defaultValue.c_str());
        char buf[64]; snprintf(buf, sizeof(buf), "%g", v);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatToStr);

    NodeTemplate intToStr;
    intToStr.type = "Int_To_String";
    intToStr.category = "Conversions";
    intToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    intToStr.execute = [](Node& node, Stargraph&) {
        int v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = atoi(p.defaultValue.c_str());
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(v);
    };
    NodeRegistry::Get().RegisterNode(intToStr);

    NodeTemplate boolToStr;
    boolToStr.type = "Bool_To_String";
    boolToStr.category = "Conversions";
    boolToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    boolToStr.execute = [](Node& node, Stargraph&) {
        bool v = false;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = (p.defaultValue == "true" || p.defaultValue == "1");
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = v ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(boolToStr);

    NodeTemplate floatToInt;
    floatToInt.type = "Float_To_Int";
    floatToInt.category = "Conversions";
    floatToInt.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Integer, false, true});
    };
    floatToInt.execute = [](Node& node, Stargraph&) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = (float)atof(p.defaultValue.c_str());
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string((int)v);
    };
    NodeRegistry::Get().RegisterNode(floatToInt);

    NodeTemplate intToFloat;
    intToFloat.type = "Int_To_Float";
    intToFloat.category = "Conversions";
    intToFloat.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    intToFloat.execute = [](Node& node, Stargraph&) {
        int v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = atoi(p.defaultValue.c_str());
        char buf[32]; snprintf(buf, sizeof(buf), "%g", (float)v);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(intToFloat);

    NodeTemplate getVar;
    getVar.type = "Get_Variable";
    getVar.category = "Variables";
    getVar.createPins = [](Node* node) {
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getVar.execute = [](Node& node, Stargraph& graph) {
        std::string varName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") { varName = p.defaultValue; break; }
        }
        for (const Variable& v : graph.GetVariables()) {
            if (v.name == varName) {
                for (Pin& p : node.outputs) {
                    if (p.name == "Value") { p.defaultValue = v.value; break; }
                }
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(getVar);

    NodeTemplate setVar;
    setVar.type = "Set_Variable";
    setVar.category = "Variables";
    setVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Value",         PinType::Float,  true,  false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVar.execute = [](Node& node, Stargraph& graph) {
        std::string varName;
        std::string value;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") varName = p.defaultValue;
            if (p.name == "Value")         value   = p.defaultValue;
        }
        for (Variable& v : graph.GetVariables()) {
            if (v.name == varName) { v.value = value; break; }
        }
    };
    NodeRegistry::Get().RegisterNode(setVar);
}

void StargraphModule::RegisterAssetType() {
    AssetTypeInfo info;
    info.typeName = "Actor";
    info.extension = ".staractor";
    info.category = "Gameplay";
    info.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        Stargraph graph;
        graph.assetName = assetName;
        graph.parentClass = "Actor";
        return graph.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(info);
}