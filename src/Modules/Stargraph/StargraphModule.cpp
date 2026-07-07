#include "StargraphModule.h"
#include "NodeRegistry.h"
#include "ExecutionContext.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Components/CameraComponent.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/CollisionComponent.h"
#include "Runtime/Components/AudioComponent.h"
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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->position.x, target->position.y, target->position.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorLocation);

    NodeTemplate setActorLocation;
    setActorLocation.type = "Set_Actor_Location";
    setActorLocation.category = "Actor";
    setActorLocation.moduleName = "Stargraph";
    setActorLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Target",   PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Location", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Location") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->position.x, &target->position.y, &target->position.z);
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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Forward", PinType::Vec3, false, true});
    };
    getActorForward.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        float yr = target->rotation.y * DEG2RAD;
        float pr = target->rotation.x * DEG2RAD;
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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            target->rotation.x, target->rotation.y, target->rotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorRotation);

    NodeTemplate setActorRotation;
    setActorRotation.type = "Set_Actor_Rotation";
    setActorRotation.category = "Actor";
    setActorRotation.moduleName = "Stargraph";
    setActorRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Target",   PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Rotation", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Rotation") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->rotation.x, &target->rotation.y, &target->rotation.z);
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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            target->scale.x, target->scale.y, target->scale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorScale);

    NodeTemplate setActorScale;
    setActorScale.type = "Set_Actor_Scale";
    setActorScale.category = "Actor";
    setActorScale.moduleName = "Stargraph";
    setActorScale.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Target", PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Scale", PinType::Vec3, true,  false, 0, "1,1,1"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    setActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Scale") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->scale.x, &target->scale.y, &target->scale.z);
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
        node->outputs.push_back({"Axis",       PinType::Float,   false, true});
        node->outputs.push_back({"Input Time", PinType::Float,   false, true});
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
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
    };
    destroyActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (target) World::Get().DestroyActor(target);
    };
    NodeRegistry::Get().RegisterNode(destroyActor);

    NodeTemplate getComponent;
    getComponent.type = "Get_Component";
    getComponent.category = "Components";
    getComponent.moduleName = "Stargraph";
    getComponent.hideFromContextMenu = true;
    getComponent.createPins = [](Node* node) {
        node->inputs.push_back({"Component_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Component", PinType::Object, false, true});
    };
    getComponent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string compName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Component_Name") { compName = p.defaultValue; break; }
        }
        if (!ctx.actor || compName.empty()) return;
        Component* comp = ctx.actor->FindComponentByName(compName);
        if (!comp) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%p", (void*)comp);
        for (Pin& p : node.outputs) {
            if (p.name == "Component") { p.defaultValue = buf; break; }
        }
    };
    NodeRegistry::Get().RegisterNode(getComponent);

    NodeTemplate setCompLocation;
    setCompLocation.type = "Set_Relative_Location";
    setCompLocation.category = "Components";
    setCompLocation.moduleName = "Stargraph";
    setCompLocation.hideFromContextMenu = true;
    setCompLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Location", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 loc = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Location") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &loc.x, &loc.y, &loc.z);
        }
        if (!target) return;
        target->position = loc;
    };
    NodeRegistry::Get().RegisterNode(setCompLocation);

    NodeTemplate setCompRotation;
    setCompRotation.type = "Set_Relative_Rotation";
    setCompRotation.category = "Components";
    setCompRotation.moduleName = "Stargraph";
    setCompRotation.hideFromContextMenu = true;
    setCompRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Rotation", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 rot = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Rotation") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &rot.x, &rot.y, &rot.z);
        }
        if (!target) return;
        target->rotation = rot;
    };
    NodeRegistry::Get().RegisterNode(setCompRotation);

    NodeTemplate setCompScale;
    setCompScale.type = "Set_Relative_Scale";
    setCompScale.category = "Components";
    setCompScale.moduleName = "Stargraph";
    setCompScale.hideFromContextMenu = true;
    setCompScale.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Scale", PinType::Vec3, true, false, 0, "1,1,1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        Vector3 scl = {1, 1, 1};
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Scale") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &scl.x, &scl.y, &scl.z);
        }
        if (!target) return;
        target->scale = scl;
    };
    NodeRegistry::Get().RegisterNode(setCompScale);

    NodeTemplate getCompLocation;
    getCompLocation.type = "Get_Relative_Location";
    getCompLocation.category = "Components";
    getCompLocation.moduleName = "Stargraph";
    getCompLocation.hideFromContextMenu = true;
    getCompLocation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->position.x, target->position.y, target->position.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompLocation);

    NodeTemplate getCompRotation;
    getCompRotation.type = "Get_Relative_Rotation";
    getCompRotation.category = "Components";
    getCompRotation.moduleName = "Stargraph";
    getCompRotation.hideFromContextMenu = true;
    getCompRotation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getCompRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->rotation.x, target->rotation.y, target->rotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompRotation);

    NodeTemplate getCompScale;
    getCompScale.type = "Get_Relative_Scale";
    getCompScale.category = "Components";
    getCompScale.moduleName = "Stargraph";
    getCompScale.hideFromContextMenu = true;
    getCompScale.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getCompScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->scale.x, target->scale.y, target->scale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompScale);

    // World location, actor scale used as offset multiplier since components have no rotation composition yet
    NodeTemplate getCompWorldLocation;
    getCompWorldLocation.type = "Get_World_Location";
    getCompWorldLocation.category = "Components";
    getCompWorldLocation.moduleName = "Stargraph";
    getCompWorldLocation.hideFromContextMenu = true;
    getCompWorldLocation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getCompWorldLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
        }
        if (!target) return;
        Vector3 worldLoc = Vector3Add(ctx.actor->position, Vector3Multiply(target->position, ctx.actor->scale));
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", worldLoc.x, worldLoc.y, worldLoc.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getCompWorldLocation);

    RegisterCameraComponentNodes();
    RegisterMeshComponentNodes();
    RegisterCollisionComponentNodes();
    RegisterAudioComponentNodes();
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