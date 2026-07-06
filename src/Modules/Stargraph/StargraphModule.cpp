#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "ExecutionContext.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Stargraph.h"
#include "InputKeyNames.h"
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

static float GetScalarValue(const Pin& p) {
    if (p.type == PinType::Boolean) return (p.defaultValue == "true" || p.defaultValue == "1") ? 1.0f : 0.0f;
    if (p.type == PinType::Integer) return (float)atoi(p.defaultValue.c_str());
    return (float)atof(p.defaultValue.c_str());
}

void StargraphModule::RegisterDefaultNodes() {

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

    NodeTemplate branch;
    branch.type = "Branch";
    branch.category = "Flow";
    branch.moduleName = "Stargraph";
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
    forLoop.moduleName = "Stargraph";
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
    whileLoop.moduleName = "Stargraph";
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
    sequence.moduleName = "Stargraph";
    sequence.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"0", PinType::Exec, false, true});
        node->outputs.push_back({"1", PinType::Exec, false, true});
        node->outputs.push_back({"2", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(sequence);

    // Math nodes - wildcard with mixed scalar support
    auto RegisterWildcardMath = [](const std::string& nodeType, char op) {
        NodeTemplate t;
        t.type = nodeType;
        t.category = "Math";
        t.moduleName = "Stargraph";
        t.createPins = [](Node* node) {
            node->inputs.push_back({"A", PinType::Wildcard, true, false, 0, "0"});
            node->inputs.push_back({"B", PinType::Wildcard, true, false, 0, "0"});
            node->outputs.push_back({"Result", PinType::Wildcard, false, true});
        };
        t.execute = [op](Node& node, Stargraph& graph, ExecutionContext& ctx) {
            PinType typeA = PinType::Float, typeB = PinType::Float;
            for (const Pin& p : node.inputs) {
                if (p.name == "A") {
                    for (const Link& lk : graph.GetLinks()) {
                        if (lk.inputPinId == p.id) {
                            for (const Node& n : graph.GetNodes()) {
                                for (const Pin& op : n.outputs) if (op.id == lk.outputPinId) { typeA = op.type; break; }
                            }
                            break;
                        }
                    }
                }
                if (p.name == "B") {
                    for (const Link& lk : graph.GetLinks()) {
                        if (lk.inputPinId == p.id) {
                            for (const Node& n : graph.GetNodes()) {
                                for (const Pin& op : n.outputs) if (op.id == lk.outputPinId) { typeB = op.type; break; }
                            }
                            break;
                        }
                    }
                }
            }

            bool aIsVec = (typeA == PinType::Vec3), bIsVec = (typeB == PinType::Vec3);
            PinType resultType = (aIsVec || bIsVec) ? PinType::Vec3 : 
                                (typeA == PinType::Integer || typeB == PinType::Integer) ? PinType::Integer : PinType::Float;

            if (resultType == PinType::Vec3) {
                Vector3 aVec = {0,0,0}, bVec = {0,0,0};
                float aScalar = 0, bScalar = 0;
                bool aIsScalar = !aIsVec, bIsScalar = !bIsVec;
                for (const Pin& p : node.inputs) {
                    if (p.name == "A") {
                        if (aIsVec) sscanf(p.defaultValue.c_str(), "%f,%f,%f", &aVec.x, &aVec.y, &aVec.z);
                        else aScalar = GetScalarValue(p);
                    }
                    if (p.name == "B") {
                        if (bIsVec) sscanf(p.defaultValue.c_str(), "%f,%f,%f", &bVec.x, &bVec.y, &bVec.z);
                        else bScalar = GetScalarValue(p);
                    }
                }
                Vector3 r = {0,0,0};
                if (aIsScalar && bIsScalar) {
                    r.x = r.y = r.z = aScalar;
                    switch (op) {
                        case '+': r.x += bScalar; break;
                        case '-': r.x -= bScalar; break;
                        case '*': r.x *= bScalar; break;
                        case '/': if (bScalar != 0) r.x /= bScalar; break;
                    }
                } else if (bIsScalar) {
                    r = aVec;
                    switch (op) {
                        case '+': r.x += bScalar; r.y += bScalar; r.z += bScalar; break;
                        case '-': r.x -= bScalar; r.y -= bScalar; r.z -= bScalar; break;
                        case '*': r.x *= bScalar; r.y *= bScalar; r.z *= bScalar; break;
                        case '/': if (bScalar != 0) { r.x /= bScalar; r.y /= bScalar; r.z /= bScalar; } break;
                    }
                } else if (aIsScalar) {
                    r = bVec;
                    switch (op) {
                        case '+': r.x += aScalar; r.y += aScalar; r.z += aScalar; break;
                        case '-': r.x -= aScalar; r.y -= aScalar; r.z -= aScalar; break;
                        case '*': r.x *= aScalar; r.y *= aScalar; r.z *= aScalar; break;
                        case '/': if (aScalar != 0) { r.x /= aScalar; r.y /= aScalar; r.z /= aScalar; } break;
                    }
                } else {
                    switch (op) {
                        case '+': r = Vector3Add(aVec, bVec); break;
                        case '-': r = Vector3Subtract(aVec, bVec); break;
                        case '*': r = Vector3Multiply(aVec, bVec); break;
                        case '/': r = Vector3Divide(aVec, bVec); break;
                    }
                }
                char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
            } else if (resultType == PinType::Integer) {
                int a = 0, b = 0;
                for (const Pin& p : node.inputs) {
                    if (p.name == "A") a = (int)GetScalarValue(p);
                    if (p.name == "B") b = (int)GetScalarValue(p);
                }
                int result = 0;
                switch (op) {
                    case '+': result = a + b; break;
                    case '-': result = a - b; break;
                    case '*': result = a * b; break;
                    case '/': result = (b != 0) ? a / b : 0; break;
                }
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(result);
            } else {
                float a = 0, b = 0;
                for (const Pin& p : node.inputs) {
                    if (p.name == "A") a = GetScalarValue(p);
                    if (p.name == "B") b = GetScalarValue(p);
                }
                float result = 0;
                switch (op) {
                    case '+': result = a + b; break;
                    case '-': result = a - b; break;
                    case '*': result = a * b; break;
                    case '/': result = (b != 0) ? a / b : 0; break;
                }
                char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
            }
        };
        NodeRegistry::Get().RegisterNode(t);
    };

    RegisterWildcardMath("Add",      '+');
    RegisterWildcardMath("Subtract", '-');
    RegisterWildcardMath("Multiply", '*');
    RegisterWildcardMath("Divide",   '/');

    NodeTemplate floatToStr;
    floatToStr.type = "Float_To_String";
    floatToStr.category = "Conversions";
    floatToStr.moduleName = "Stargraph";
    floatToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    floatToStr.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = (float)atof(p.defaultValue.c_str());
        char buf[64]; snprintf(buf, sizeof(buf), "%g", v);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatToStr);

    NodeTemplate intToStr;
    intToStr.type = "Int_To_String";
    intToStr.category = "Conversions";
    intToStr.moduleName = "Stargraph";
    intToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    intToStr.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = atoi(p.defaultValue.c_str());
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(v);
    };
    NodeRegistry::Get().RegisterNode(intToStr);

    NodeTemplate boolToStr;
    boolToStr.type = "Bool_To_String";
    boolToStr.category = "Conversions";
    boolToStr.moduleName = "Stargraph";
    boolToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    boolToStr.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool v = false;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = (p.defaultValue == "true" || p.defaultValue == "1");
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = v ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(boolToStr);

    NodeTemplate floatToInt;
    floatToInt.type = "Float_To_Int";
    floatToInt.category = "Conversions";
    floatToInt.moduleName = "Stargraph";
    floatToInt.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Integer, false, true});
    };
    floatToInt.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = (float)atof(p.defaultValue.c_str());
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string((int)v);
    };
    NodeRegistry::Get().RegisterNode(floatToInt);

    NodeTemplate intToFloat;
    intToFloat.type = "Int_To_Float";
    intToFloat.category = "Conversions";
    intToFloat.moduleName = "Stargraph";
    intToFloat.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Integer, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    intToFloat.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = atoi(p.defaultValue.c_str());
        char buf[32]; snprintf(buf, sizeof(buf), "%g", (float)v);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(intToFloat);

    NodeTemplate getVar;
    getVar.type = "Get_Variable";
    getVar.category = "Variables";
    getVar.moduleName = "Stargraph";
    getVar.createPins = [](Node* node) {
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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
    setVar.moduleName = "Stargraph";
    setVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Value",         PinType::Float,  true,  false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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

    // Actor self-reference nodes

    NodeTemplate getActorLocation;
    getActorLocation.type = "Get_Actor_Location";
    getActorLocation.category = "Actor";
    getActorLocation.moduleName = "Stargraph";
    getActorLocation.createPins = [](Node* node) {
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", ctx.actor->position.x, ctx.actor->position.y, ctx.actor->position.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorLocation);

    NodeTemplate setActorLocation;
    setActorLocation.type = "Set_Actor_Location";
    setActorLocation.category = "Actor";
    setActorLocation.moduleName = "Stargraph";
    setActorLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Location", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Location") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &ctx.actor->position.x, &ctx.actor->position.y, &ctx.actor->position.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorLocation);

    NodeTemplate getActorForward;
    getActorForward.type = "Get_Actor_Forward_Vector";
    getActorForward.category = "Actor";
    getActorForward.moduleName = "Stargraph";
    getActorForward.createPins = [](Node* node) {
        node->outputs.push_back({"Forward", PinType::Vec3, false, true});
    };
    getActorForward.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        float yr = ctx.actor->rotation.y * DEG2RAD;
        float pr = ctx.actor->rotation.x * DEG2RAD;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr));
        for (Pin& p : node.outputs) if (p.name == "Forward") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorForward);

    NodeTemplate vec3Scale;
    vec3Scale.type = "Vec3_Scale";
    vec3Scale.category = "Math";
    vec3Scale.moduleName = "Stargraph";
    vec3Scale.createPins = [](Node* node) {
        node->inputs.push_back({"Vector", PinType::Vec3,  true, false, 0, "0,0,0"});
        node->inputs.push_back({"Scale",  PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    vec3Scale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 v = {0, 0, 0};
        float s = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Vector") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &v.x, &v.y, &v.z);
            if (p.name == "Scale" && !p.defaultValue.empty()) s = (float)atof(p.defaultValue.c_str());
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", v.x * s, v.y * s, v.z * s);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Scale);

    NodeTemplate vec3Make;
    vec3Make.type = "Vec3_Make";
    vec3Make.category = "Math";
    vec3Make.moduleName = "Stargraph";
    vec3Make.createPins = [](Node* node) {
        node->inputs.push_back({"X", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Y", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Z", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Vector", PinType::Vec3, false, true});
    };
    vec3Make.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "X" && !p.defaultValue.empty()) x = (float)atof(p.defaultValue.c_str());
            if (p.name == "Y" && !p.defaultValue.empty()) y = (float)atof(p.defaultValue.c_str());
            if (p.name == "Z" && !p.defaultValue.empty()) z = (float)atof(p.defaultValue.c_str());
        }
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", x, y, z);
        for (Pin& p : node.outputs) if (p.name == "Vector") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Make);

    NodeTemplate vec3Break;
    vec3Break.type = "Vec3_Break";
    vec3Break.category = "Math";
    vec3Break.moduleName = "Stargraph";
    vec3Break.createPins = [](Node* node) {
        node->inputs.push_back({"Vector", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"X", PinType::Float, false, true});
        node->outputs.push_back({"Y", PinType::Float, false, true});
        node->outputs.push_back({"Z", PinType::Float, false, true});
    };
    vec3Break.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "Vector") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &x, &y, &z);
        }
        char buf[32];
        for (Pin& p : node.outputs) {
            if (p.name == "X") { snprintf(buf, sizeof(buf), "%g", x); p.defaultValue = buf; }
            if (p.name == "Y") { snprintf(buf, sizeof(buf), "%g", y); p.defaultValue = buf; }
            if (p.name == "Z") { snprintf(buf, sizeof(buf), "%g", z); p.defaultValue = buf; }
        }
    };
    NodeRegistry::Get().RegisterNode(vec3Break);

    NodeTemplate getActorRotation;
    getActorRotation.type = "Get_Actor_Rotation";
    getActorRotation.category = "Actor";
    getActorRotation.moduleName = "Stargraph";
    getActorRotation.createPins = [](Node* node) {
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            ctx.actor->rotation.x, ctx.actor->rotation.y, ctx.actor->rotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorRotation);

    NodeTemplate setActorRotation;
    setActorRotation.type = "Set_Actor_Rotation";
    setActorRotation.category = "Actor";
    setActorRotation.moduleName = "Stargraph";
    setActorRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Rotation", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Rotation") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &ctx.actor->rotation.x, &ctx.actor->rotation.y, &ctx.actor->rotation.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorRotation);

    NodeTemplate getActorScale;
    getActorScale.type = "Get_Actor_Scale";
    getActorScale.category = "Actor";
    getActorScale.moduleName = "Stargraph";
    getActorScale.createPins = [](Node* node) {
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            ctx.actor->scale.x, ctx.actor->scale.y, ctx.actor->scale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorScale);

    NodeTemplate setActorScale;
    setActorScale.type = "Set_Actor_Scale";
    setActorScale.category = "Actor";
    setActorScale.moduleName = "Stargraph";
    setActorScale.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Scale", PinType::Vec3, true,  false, 0, "1,1,1"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    setActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Scale") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &ctx.actor->scale.x, &ctx.actor->scale.y, &ctx.actor->scale.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorScale);

    NodeTemplate inputKey;
    inputKey.type = "Input_Key";
    inputKey.category = "Input";
    inputKey.moduleName = "Stargraph";
    inputKey.isEvent = true;
    inputKey.createPins = [](Node* node) {
        node->inputs.push_back({"Key", PinType::String, true, false, 0, "W"});
        node->outputs.push_back({"Triggered",  PinType::Exec,    false, true});
        node->outputs.push_back({"Started",    PinType::Exec,    false, true});
        node->outputs.push_back({"Completed",  PinType::Exec,    false, true});
        node->outputs.push_back({"Is Held",    PinType::Boolean, false, true});
        node->outputs.push_back({"Input Time", PinType::Float,   false, true});
    };
    inputKey.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string keyName;
        for (const Pin& p : node.inputs) if (p.name == "Key") keyName = p.defaultValue;
        int key = InputKeyNameToCode(keyName);
        bool held = key != 0 && IsKeyDown(key);
        float inputTime = held ? (ctx.deltaTime * 120.0f) : 0.0f;
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%g", inputTime);
        for (Pin& p : node.outputs) {
            if (p.name == "Is Held")    p.defaultValue = held ? "true" : "false";
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

    NodeTemplate spawnActor;
    spawnActor.type = "Spawn_Actor";
    spawnActor.category = "Actor";
    spawnActor.moduleName = "Stargraph";
    spawnActor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Class Name", PinType::String, true, false, 0, "Actor"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Spawned Actor", PinType::Object, false, true});
    };
    spawnActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string className = "Actor";
        for (const Pin& p : node.inputs) {
            if (p.name == "Class Name") { className = p.defaultValue; break; }
        }
        Actor* spawned = World::Get().SpawnActor(className);
        if (spawned) {
            spawned->SetName(className + "_Instance");
            for (Pin& p : node.outputs) {
                if (p.name == "Spawned Actor") {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%p", (void*)spawned);
                    p.defaultValue = buf;
                    break;
                }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(spawnActor);

    NodeTemplate destroyActor;
    destroyActor.type = "Destroy_Actor";
    destroyActor.category = "Actor";
    destroyActor.moduleName = "Stargraph";
    destroyActor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
    };
    destroyActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        for (const Pin& p : node.inputs) {
            if (p.name == "Target") {
                Actor* target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                if (target) World::Get().DestroyActor(target);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(destroyActor);
}

void StargraphModule::RegisterAssetType() {
    AssetTypeInfo info;
    info.typeName = "Actor";
    info.extension = ".staractor";
    info.category = "Gameplay";
    info.moduleName = "Stargraph";
    info.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        Stargraph graph;
        graph.assetName = assetName;
        graph.parentClass = "Actor";
        return graph.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(info);
}