#include "Nodes_Math.h"
#include "NodeRegistry.h"
#include "raylib.h"
#include "raymath.h"
#include <cstdlib>
#include <ctime>

static float GetScalarValue(const Pin& p) {
    if (p.type == PinType::Boolean) return p.defaultValue.AsBool() ? 1.0f : 0.0f;
    if (p.type == PinType::Integer) return (float)p.defaultValue.AsInt();
    return p.defaultValue.AsFloat();
}

void RegisterMathNodes() {
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
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) typeA = t;
                }
                if (p.name == "B") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) typeB = t;
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
                        if (aIsVec) ReadVec3(p, aVec.x, aVec.y, aVec.z);
                        else aScalar = GetScalarValue(p);
                    }
                    if (p.name == "B") {
                        if (bIsVec) ReadVec3(p, bVec.x, bVec.y, bVec.z);
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
                        case '/':
                            r.x = bVec.x != 0.0f ? aVec.x / bVec.x : 0.0f;
                            r.y = bVec.y != 0.0f ? aVec.y / bVec.y : 0.0f;
                            r.z = bVec.z != 0.0f ? aVec.z / bVec.z : 0.0f;
                            break;
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

    NodeTemplate mathPower;
    mathPower.type = "Power";
    mathPower.category = "Math";
    mathPower.moduleName = "Stargraph";
    mathPower.createPins = [](Node* node) {
        node->inputs.push_back({"Base", PinType::Float, true, false, 0, "1.0"});
        node->inputs.push_back({"Exponent", PinType::Float, true, false, 0, "2.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    mathPower.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float base = 1.0f, exponent = 2.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Base") base = GetScalarValue(p);
            if (p.name == "Exponent") exponent = GetScalarValue(p);
        }
        float result = powf(base, exponent);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(mathPower);

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
            if (p.name == "X" && !p.defaultValue.empty()) x = p.defaultValue.AsFloat();
            if (p.name == "Y" && !p.defaultValue.empty()) y = p.defaultValue.AsFloat();
            if (p.name == "Z" && !p.defaultValue.empty()) z = p.defaultValue.AsFloat();
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
            if (p.name == "Vector") ReadVec3(p, x, y, z);
        }
        char buf[32];
        for (Pin& p : node.outputs) {
            if (p.name == "X") { snprintf(buf, sizeof(buf), "%g", x); p.defaultValue = buf; }
            if (p.name == "Y") { snprintf(buf, sizeof(buf), "%g", y); p.defaultValue = buf; }
            if (p.name == "Z") { snprintf(buf, sizeof(buf), "%g", z); p.defaultValue = buf; }
        }
    };
    NodeRegistry::Get().RegisterNode(vec3Break);

    NodeTemplate rotatorMake;
    rotatorMake.type = "Rotator_Make";
    rotatorMake.category = "Math";
    rotatorMake.moduleName = "Stargraph";
    rotatorMake.createPins = [](Node* node) {
        node->inputs.push_back({"X", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Y", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Z", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Rotator", PinType::Rotator, false, true});
    };
    rotatorMake.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "X" && !p.defaultValue.empty()) x = p.defaultValue.AsFloat();
            if (p.name == "Y" && !p.defaultValue.empty()) y = p.defaultValue.AsFloat();
            if (p.name == "Z" && !p.defaultValue.empty()) z = p.defaultValue.AsFloat();
        }
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", x, y, z);
        for (Pin& p : node.outputs) if (p.name == "Rotator") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(rotatorMake);

    NodeTemplate rotatorBreak;
    rotatorBreak.type = "Rotator_Break";
    rotatorBreak.category = "Math";
    rotatorBreak.moduleName = "Stargraph";
    rotatorBreak.createPins = [](Node* node) {
        node->inputs.push_back({"Rotator", PinType::Rotator, true, false, 0, "0,0,0"});
        node->outputs.push_back({"X", PinType::Float, false, true});
        node->outputs.push_back({"Y", PinType::Float, false, true});
        node->outputs.push_back({"Z", PinType::Float, false, true});
    };
    rotatorBreak.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "Rotator") ReadVec3(p, x, y, z);
        }
        char buf[32];
        for (Pin& p : node.outputs) {
            if (p.name == "X") { snprintf(buf, sizeof(buf), "%g", x); p.defaultValue = buf; }
            if (p.name == "Y") { snprintf(buf, sizeof(buf), "%g", y); p.defaultValue = buf; }
            if (p.name == "Z") { snprintf(buf, sizeof(buf), "%g", z); p.defaultValue = buf; }
        }
    };
    NodeRegistry::Get().RegisterNode(rotatorBreak);

    NodeTemplate vec3Length;
    vec3Length.type = "Vec3_Length";
    vec3Length.category = "Math";
    vec3Length.moduleName = "Stargraph";
    vec3Length.createPins = [](Node* node) {
        node->inputs.push_back({"Vector", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Length", PinType::Float, false, true});
    };
    vec3Length.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "Vector") ReadVec3(p, x, y, z);
        }
        float len = sqrtf(x*x + y*y + z*z);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", len);
        for (Pin& p : node.outputs) if (p.name == "Length") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Length);

    NodeTemplate vec3Normalize;
    vec3Normalize.type = "Vec3_Normalize";
    vec3Normalize.category = "Math";
    vec3Normalize.moduleName = "Stargraph";
    vec3Normalize.createPins = [](Node* node) {
        node->inputs.push_back({"Vector", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    vec3Normalize.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "Vector") ReadVec3(p, x, y, z);
        }
        float len = sqrtf(x*x + y*y + z*z);
        float nx = 0, ny = 0, nz = 0;
        if (len > 0) { nx = x/len; ny = y/len; nz = z/len; }
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", nx, ny, nz);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Normalize);

    NodeTemplate vec3Dot;
    vec3Dot.type = "Vec3_Dot";
    vec3Dot.category = "Math";
    vec3Dot.moduleName = "Stargraph";
    vec3Dot.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    vec3Dot.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float ax = 0, ay = 0, az = 0;
        float bx = 0, by = 0, bz = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") ReadVec3(p, ax, ay, az);
            if (p.name == "B") ReadVec3(p, bx, by, bz);
        }
        float dot = ax*bx + ay*by + az*bz;
        char buf[32]; snprintf(buf, sizeof(buf), "%g", dot);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Dot);

    NodeTemplate vec3Cross;
    vec3Cross.type = "Vec3_Cross";
    vec3Cross.category = "Math";
    vec3Cross.moduleName = "Stargraph";
    vec3Cross.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    vec3Cross.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float ax = 0, ay = 0, az = 0;
        float bx = 0, by = 0, bz = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") ReadVec3(p, ax, ay, az);
            if (p.name == "B") ReadVec3(p, bx, by, bz);
        }
        Vector3 a = {ax, ay, az};
        Vector3 b = {bx, by, bz};
        Vector3 cross = Vector3CrossProduct(a, b);
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", cross.x, cross.y, cross.z);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Cross);

    NodeTemplate vec3Distance;
    vec3Distance.type = "Vec3_Distance";
    vec3Distance.category = "Math";
    vec3Distance.moduleName = "Stargraph";
    vec3Distance.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Distance", PinType::Float, false, true});
    };
    vec3Distance.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float ax = 0, ay = 0, az = 0;
        float bx = 0, by = 0, bz = 0;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") ReadVec3(p, ax, ay, az);
            if (p.name == "B") ReadVec3(p, bx, by, bz);
        }
        float dx = ax - bx, dy = ay - by, dz = az - bz;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", dist);
        for (Pin& p : node.outputs) if (p.name == "Distance") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Distance);

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
            if (p.name == "Vector") ReadVec3(p, v.x, v.y, v.z);
            if (p.name == "Scale" && !p.defaultValue.empty()) s = p.defaultValue.AsFloat();
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", v.x * s, v.y * s, v.z * s);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Scale);

    NodeTemplate vec3NormalizedDistance;
    vec3NormalizedDistance.type = "Vec3_Normalized_Distance";
    vec3NormalizedDistance.category = "Math";
    vec3NormalizedDistance.moduleName = "Stargraph";
    vec3NormalizedDistance.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"B", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Direction", PinType::Vec3, false, true});
        node->outputs.push_back({"Distance", PinType::Float, false, true});
    };
    vec3NormalizedDistance.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 a = {0, 0, 0}, b = {0, 0, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "A") ReadVec3(p, a.x, a.y, a.z);
            if (p.name == "B") ReadVec3(p, b.x, b.y, b.z);
        }
        Vector3 delta = Vector3Subtract(b, a);
        float dist = Vector3Length(delta);
        Vector3 dir = (dist > 0) ? Vector3Scale(delta, 1.0f / dist) : Vector3{0, 0, 0};
        char buf[64];
        for (Pin& p : node.outputs) {
            if (p.name == "Direction") { snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", dir.x, dir.y, dir.z); p.defaultValue = buf; }
            if (p.name == "Distance")  { snprintf(buf, sizeof(buf), "%g", dist); p.defaultValue = buf; }
        }
    };
    NodeRegistry::Get().RegisterNode(vec3NormalizedDistance);

    NodeTemplate vec3Reflect;
    vec3Reflect.type = "Vec3_Reflect";
    vec3Reflect.category = "Math";
    vec3Reflect.moduleName = "Stargraph";
    vec3Reflect.createPins = [](Node* node) {
        node->inputs.push_back({"Vector", PinType::Vec3, true, false, 0, "0,0,0"});
        node->inputs.push_back({"Normal", PinType::Vec3, true, false, 0, "0,1,0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    vec3Reflect.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 v = {0, 0, 0}, n = {0, 1, 0};
        for (const Pin& p : node.inputs) {
            if (p.name == "Vector") ReadVec3(p, v.x, v.y, v.z);
            if (p.name == "Normal") ReadVec3(p, n.x, n.y, n.z);
        }
        Vector3 r = Vector3Reflect(v, n);
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", r.x, r.y, r.z);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3Reflect);

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
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = p.defaultValue.AsFloat();
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
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = p.defaultValue.AsInt();
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
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = p.defaultValue.AsFloat();
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
        for (const Pin& p : node.inputs) if (p.name == "Value" && !p.defaultValue.empty()) v = p.defaultValue.AsInt();
        char buf[32]; snprintf(buf, sizeof(buf), "%g", (float)v);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(intToFloat);

    NodeTemplate vec3ToStr;
    vec3ToStr.type = "Vec3_To_String";
    vec3ToStr.category = "Conversions";
    vec3ToStr.moduleName = "Stargraph";
    vec3ToStr.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Vec3, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    vec3ToStr.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") ReadVec3(p, x, y, z);
        char buf[64]; snprintf(buf, sizeof(buf), "(%g, %g, %g)", x, y, z);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(vec3ToStr);

    NodeTemplate strToVec3;
    strToVec3.type = "String_To_Vec3";
    strToVec3.category = "Conversions";
    strToVec3.moduleName = "Stargraph";
    strToVec3.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::String, true, false, 0, "0,0,0"});
        node->outputs.push_back({"Result", PinType::Vec3, false, true});
    };
    strToVec3.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float x = 0, y = 0, z = 0;
        for (const Pin& p : node.inputs) {
            if (p.name != "Value") continue;
            std::string s = p.defaultValue;
            for (char& c : s) if (c == '(' || c == ')') c = ' ';
            sscanf(s.c_str(), "%f , %f , %f", &x, &y, &z);
        }
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", x, y, z);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(strToVec3);

    NodeTemplate boolToFloat;
    boolToFloat.type = "Bool_To_Float";
    boolToFloat.category = "Conversions";
    boolToFloat.moduleName = "Stargraph";
    boolToFloat.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    boolToFloat.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool v = false;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = (p.defaultValue == "true" || p.defaultValue == "1");
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = v ? "1" : "0";
    };
    NodeRegistry::Get().RegisterNode(boolToFloat);

    NodeTemplate boolToInt;
    boolToInt.type = "Bool_To_Int";
    boolToInt.category = "Conversions";
    boolToInt.moduleName = "Stargraph";
    boolToInt.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::Integer, false, true});
    };
    boolToInt.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool v = false;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = (p.defaultValue == "true" || p.defaultValue == "1");
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = v ? "1" : "0";
    };
    NodeRegistry::Get().RegisterNode(boolToInt);

    auto RegisterWildcardFloatUnary = [](const std::string& nodeType, char op) {
        NodeTemplate t;
        t.type = nodeType;
        t.category = "Math";
        t.moduleName = "Stargraph";
        t.createPins = [](Node* node) {
            node->inputs.push_back({"Value", PinType::Wildcard, true, false, 0, "0"});
            node->outputs.push_back({"Result", PinType::Wildcard, false, true});
        };
        t.execute = [op](Node& node, Stargraph& graph, ExecutionContext& ctx) {
            PinType inType = PinType::Float;
            for (const Pin& p : node.inputs) {
                if (p.name == "Value") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) inType = t;
                }
            }

            float v = 0;
            if (inType == PinType::Integer) {
                for (const Pin& p : node.inputs) if (p.name == "Value") v = (float)p.defaultValue.AsInt();
            } else {
                for (const Pin& p : node.inputs) if (p.name == "Value") v = GetScalarValue(p);
            }

            float result = 0;
            switch (op) {
                case 'a': result = fabsf(v); break;
                case 'm': result = floorf(v); break;
                case 'c': result = ceilf(v); break;
                case 'r': result = v; if (v >= 0) result = (float)((int)(v + 0.5f)); else result = (float)((int)(v - 0.5f)); break;
            }

            if (inType == PinType::Integer) {
                int res = (int)result;
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(res);
            } else {
                char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
            }
        };
        NodeRegistry::Get().RegisterNode(t);
    };

    RegisterWildcardFloatUnary("Abs", 'a');

    NodeTemplate floatFloor;
    floatFloor.type = "Float_Floor";
    floatFloor.category = "Math";
    floatFloor.moduleName = "Stargraph";
    floatFloor.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    floatFloor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = GetScalarValue(p);
        float result = floorf(v);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatFloor);

    NodeTemplate floatCeil;
    floatCeil.type = "Float_Ceil";
    floatCeil.category = "Math";
    floatCeil.moduleName = "Stargraph";
    floatCeil.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    floatCeil.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = GetScalarValue(p);
        float result = ceilf(v);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatCeil);

    NodeTemplate floatRound;
    floatRound.type = "Float_Round";
    floatRound.category = "Math";
    floatRound.moduleName = "Stargraph";
    floatRound.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    floatRound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = GetScalarValue(p);
        float result = v;
        if (v >= 0) result = (float)((int)(v + 0.5f)); else result = (float)((int)(v - 0.5f));
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatRound);

    auto RegisterWildcardFloatBinary = [](const std::string& nodeType, char op) {
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
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) typeA = t;
                }
                if (p.name == "B") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) typeB = t;
                }
            }

            float a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A") a = GetScalarValue(p);
                if (p.name == "B") b = GetScalarValue(p);
            }

            float result = 0;
            switch (op) {
                case 'c': result = fminf(a, b); break;
                case 'm': result = fmaxf(a, b); break;
            }

            PinType resultType = (typeA == PinType::Integer || typeB == PinType::Integer) ? PinType::Integer : PinType::Float;
            if (resultType == PinType::Integer) {
                int res = (int)result;
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(res);
            } else {
                char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
            }
        };
        NodeRegistry::Get().RegisterNode(t);
    };

    RegisterWildcardFloatBinary("Min", 'm');
    RegisterWildcardFloatBinary("Max", 'c');

    auto RegisterWildcardClamp = []() {
        NodeTemplate t;
        t.type = "Clamp";
        t.category = "Math";
        t.moduleName = "Stargraph";
        t.createPins = [](Node* node) {
            node->inputs.push_back({"Value", PinType::Wildcard, true, false, 0, "0"});
            node->inputs.push_back({"Min", PinType::Wildcard, true, false, 0, "0"});
            node->inputs.push_back({"Max", PinType::Wildcard, true, false, 0, "1"});
            node->outputs.push_back({"Result", PinType::Wildcard, false, true});
        };
        t.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
            PinType valueType = PinType::Float, minType = PinType::Float, maxType = PinType::Float;
            for (const Pin& p : node.inputs) {
                if (p.name == "Value") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) valueType = t;
                }
                if (p.name == "Min") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) minType = t;
                }
                if (p.name == "Max") {
                    PinType t = graph.GetConnectedOutputType(p.id);
                    if (t != PinType::None) maxType = t;
                }
            }

            float v = 0, minVal = 0, maxVal = 1;
            for (const Pin& p : node.inputs) {
                if (p.name == "Value") v = GetScalarValue(p);
                if (p.name == "Min") minVal = GetScalarValue(p);
                if (p.name == "Max") maxVal = GetScalarValue(p);
            }
            float result = fmaxf(minVal, fminf(maxVal, v));

            PinType resultType = (valueType == PinType::Integer || minType == PinType::Integer || maxType == PinType::Integer) ? PinType::Integer : PinType::Float;
            if (resultType == PinType::Integer) {
                int res = (int)result;
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(res);
            } else {
                char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
                for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
            }
        };
        NodeRegistry::Get().RegisterNode(t);
    };

    RegisterWildcardClamp();

    NodeTemplate floatLerp;
    floatLerp.type = "Float_Lerp";
    floatLerp.category = "Math";
    floatLerp.moduleName = "Stargraph";
    floatLerp.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"B", PinType::Float, true, false, 0, "1.0"});
        node->inputs.push_back({"Alpha", PinType::Float, true, false, 0, "0.5"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    floatLerp.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float a = 0, b = 1, alpha = 0.5;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") a = GetScalarValue(p);
            if (p.name == "B") b = GetScalarValue(p);
            if (p.name == "Alpha") alpha = GetScalarValue(p);
        }
        float result = a + (b - a) * alpha;
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatLerp);

    NodeTemplate intModulo;
    intModulo.type = "Int_Modulo";
    intModulo.category = "Math";
    intModulo.moduleName = "Stargraph";
    intModulo.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Integer, true, false, 0, "0"});
        node->inputs.push_back({"B", PinType::Integer, true, false, 0, "1"});
        node->outputs.push_back({"Result", PinType::Integer, false, true});
    };
    intModulo.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int a = 0, b = 1;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") a = p.defaultValue.AsInt();
            if (p.name == "B") b = p.defaultValue.AsInt();
        }
        int result = (b != 0) ? a % b : 0;
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = std::to_string(result);
    };
    NodeRegistry::Get().RegisterNode(intModulo);

    NodeTemplate floatTruncate;
    floatTruncate.type = "Float_Truncate";
    floatTruncate.category = "Math";
    floatTruncate.moduleName = "Stargraph";
    floatTruncate.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Result", PinType::Float, false, true});
    };
    floatTruncate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float v = 0;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = GetScalarValue(p);
        float result = (float)((int)v);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatTruncate);

    NodeTemplate floatRandom;
    floatRandom.type = "Random_Float";
    floatRandom.category = "Math";
    floatRandom.moduleName = "Stargraph";
    static bool seeded = false;
    if (!seeded) { srand((unsigned int)time(0)); seeded = true; }
    floatRandom.createPins = [](Node* node) {
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    floatRandom.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float result = (float)rand() / (float)RAND_MAX;
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Value") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatRandom);

    NodeTemplate floatRandomRange;
    floatRandomRange.type = "Random_Float_in_Range";
    floatRandomRange.category = "Math";
    floatRandomRange.moduleName = "Stargraph";
    floatRandomRange.createPins = [](Node* node) {
        node->inputs.push_back({"Min", PinType::Float, true, false, 0, "0.0"});
        node->inputs.push_back({"Max", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    floatRandomRange.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float min = 0, max = 1;
        for (const Pin& p : node.inputs) {
            if (p.name == "Min") min = GetScalarValue(p);
            if (p.name == "Max") max = GetScalarValue(p);
        }
        float result = min + (float)rand() / (float)RAND_MAX * (max - min);
        char buf[32]; snprintf(buf, sizeof(buf), "%g", result);
        for (Pin& p : node.outputs) if (p.name == "Value") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(floatRandomRange);

    NodeTemplate intRandom;
    intRandom.type = "Random_Int";
    intRandom.category = "Math";
    intRandom.moduleName = "Stargraph";
    intRandom.createPins = [](Node* node) {
        node->outputs.push_back({"Value", PinType::Integer, false, true});
    };
    intRandom.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int result = rand();
        for (Pin& p : node.outputs) if (p.name == "Value") p.defaultValue = std::to_string(result);
    };
    NodeRegistry::Get().RegisterNode(intRandom);

    NodeTemplate intRandomRange;
    intRandomRange.type = "Random_Int_in_Range";
    intRandomRange.category = "Math";
    intRandomRange.moduleName = "Stargraph";
    intRandomRange.createPins = [](Node* node) {
        node->inputs.push_back({"Min", PinType::Integer, true, false, 0, "0"});
        node->inputs.push_back({"Max", PinType::Integer, true, false, 0, "100"});
        node->outputs.push_back({"Value", PinType::Integer, false, true});
    };
    intRandomRange.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int min = 0, max = 100;
        for (const Pin& p : node.inputs) {
            if (p.name == "Min") min = p.defaultValue.AsInt();
            if (p.name == "Max") max = p.defaultValue.AsInt();
        }
        int result = min + rand() % (max - min + 1);
        for (Pin& p : node.outputs) if (p.name == "Value") p.defaultValue = std::to_string(result);
    };
    NodeRegistry::Get().RegisterNode(intRandomRange);

    NodeTemplate boolRandom;
    boolRandom.type = "Random_Bool";
    boolRandom.category = "Math";
    boolRandom.moduleName = "Stargraph";
    boolRandom.createPins = [](Node* node) {
        node->outputs.push_back({"Value", PinType::Boolean, false, true});
    };
    boolRandom.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool result = (rand() % 2) == 0;
        for (Pin& p : node.outputs) if (p.name == "Value") p.defaultValue = result ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(boolRandom);

    NodeTemplate boolNot;
    boolNot.type = "Bool_Not";
    boolNot.category = "Math";
    boolNot.moduleName = "Stargraph";
    boolNot.createPins = [](Node* node) {
        node->inputs.push_back({"Value", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::Boolean, false, true});
    };
    boolNot.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool v = false;
        for (const Pin& p : node.inputs) if (p.name == "Value") v = (p.defaultValue == "true" || p.defaultValue == "1");
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = v ? "false" : "true";
    };
    NodeRegistry::Get().RegisterNode(boolNot);

    NodeTemplate boolAnd;
    boolAnd.type = "Bool_And";
    boolAnd.category = "Math";
    boolAnd.moduleName = "Stargraph";
    boolAnd.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Boolean, true, false, 0, "false"});
        node->inputs.push_back({"B", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::Boolean, false, true});
    };
    boolAnd.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool a = false, b = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") a = (p.defaultValue == "true" || p.defaultValue == "1");
            if (p.name == "B") b = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = (a && b) ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(boolAnd);

    NodeTemplate boolOr;
    boolOr.type = "Bool_Or";
    boolOr.category = "Math";
    boolOr.moduleName = "Stargraph";
    boolOr.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Boolean, true, false, 0, "false"});
        node->inputs.push_back({"B", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Result", PinType::Boolean, false, true});
    };
    boolOr.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        bool a = false, b = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") a = (p.defaultValue == "true" || p.defaultValue == "1");
            if (p.name == "B") b = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = (a || b) ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(boolOr);

    NodeTemplate wildcardEquals;
    wildcardEquals.type = "Wildcard_Equals";
    wildcardEquals.category = "Comparisons";
    wildcardEquals.moduleName = "Stargraph";
    wildcardEquals.createPins = [](Node* node) {
        node->inputs.push_back({"A", PinType::Wildcard, true, false, 0, "0"});
        node->inputs.push_back({"B", PinType::Wildcard, true, false, 0, "0"});
        node->outputs.push_back({"Result", PinType::Boolean, false, true});
    };
    wildcardEquals.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        PinType typeA = PinType::Float, typeB = PinType::Float;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") {
                PinType t = graph.GetConnectedOutputType(p.id);
                if (t != PinType::None) typeA = t;
            }
            if (p.name == "B") {
                PinType t = graph.GetConnectedOutputType(p.id);
                if (t != PinType::None) typeB = t;
            }
        }

        PinType cmpType = (typeA != PinType::Float) ? typeA : typeB;
        bool result = false;
        std::string aStr, bStr;
        for (const Pin& p : node.inputs) {
            if (p.name == "A") aStr = p.defaultValue;
            if (p.name == "B") bStr = p.defaultValue;
        }

        if (cmpType == PinType::Vec3) {
            Vector3 a = {0,0,0}, b = {0,0,0};
            sscanf(aStr.c_str(), "%f,%f,%f", &a.x, &a.y, &a.z);
            sscanf(bStr.c_str(), "%f,%f,%f", &b.x, &b.y, &b.z);
            result = fabsf(a.x-b.x) < 0.0001f && fabsf(a.y-b.y) < 0.0001f && fabsf(a.z-b.z) < 0.0001f;
        } else if (cmpType == PinType::String || cmpType == PinType::Object) {
            result = aStr == bStr;
        } else if (cmpType == PinType::Boolean) {
            bool a = (aStr == "true" || aStr == "1");
            bool b = (bStr == "true" || bStr == "1");
            result = a == b;
        } else if (cmpType == PinType::Integer) {
            result = atoi(aStr.c_str()) == atoi(bStr.c_str());
        } else {
            result = fabsf((float)atof(aStr.c_str()) - (float)atof(bStr.c_str())) < 0.0001f;
        }

        for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = result ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(wildcardEquals);

    auto RegisterWildcardCompare = [](const std::string& nodeType, char op) {
        NodeTemplate t;
        t.type = nodeType;
        t.category = "Comparisons";
        t.moduleName = "Stargraph";
        t.createPins = [](Node* node) {
            node->inputs.push_back({"A", PinType::Wildcard, true, false, 0, "0"});
            node->inputs.push_back({"B", PinType::Wildcard, true, false, 0, "0"});
            node->outputs.push_back({"Result", PinType::Boolean, false, true});
        };
        t.execute = [op](Node& node, Stargraph& graph, ExecutionContext& ctx) {
            float a = 0, b = 0;
            for (const Pin& p : node.inputs) {
                if (p.name == "A") a = GetScalarValue(p);
                if (p.name == "B") b = GetScalarValue(p);
            }
            bool result = false;
            switch (op) {
                case '>': result = a > b; break;
                case '<': result = a < b; break;
                case 'g': result = a >= b; break;
                case 'l': result = a <= b; break;
            }
            for (Pin& p : node.outputs) if (p.name == "Result") p.defaultValue = result ? "true" : "false";
        };
        NodeRegistry::Get().RegisterNode(t);
    };

    RegisterWildcardCompare("Greater_Than",    '>');
    RegisterWildcardCompare("Less_Than",       '<');
    RegisterWildcardCompare("Greater_Or_Equal", 'g');
    RegisterWildcardCompare("Less_Or_Equal",    'l');

    NodeTemplate alignToGround;
    alignToGround.type = "Align_To_Ground";
    alignToGround.category = "Math";
    alignToGround.moduleName = "Stargraph";
    alignToGround.createPins = [](Node* node) {
        node->inputs.push_back({"Forward", PinType::Vec3, true, false, 0, "0,0,1"});
        node->inputs.push_back({"Right", PinType::Vec3, true, false, 0, "1,0,0"});
        node->inputs.push_back({"Ground Normal", PinType::Vec3, true, false, 0, "0,1,0"});
        node->outputs.push_back({"Rotation", PinType::Rotator, false, true});
    };
    alignToGround.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 forward = {0, 0, 1};
        Vector3 right = {1, 0, 0};
        Vector3 normal = {0, 1, 0};

        for (const Pin& p : node.inputs) {
            if (p.name == "Forward") ReadVec3(p, forward.x, forward.y, forward.z);
            if (p.name == "Right") ReadVec3(p, right.x, right.y, right.z);
            if (p.name == "Ground Normal") ReadVec3(p, normal.x, normal.y, normal.z);
        }

        forward = Vector3Normalize(forward);
        right = Vector3Normalize(right);
        normal = Vector3Normalize(normal);

        Vector3 newForward = Vector3Subtract(forward, Vector3Scale(normal, Vector3DotProduct(forward, normal)));
        if (Vector3Length(newForward) < 0.001f) {
            newForward = Vector3Subtract(right, Vector3Scale(normal, Vector3DotProduct(right, normal)));
        }
        if (Vector3Length(newForward) < 0.001f) newForward = {0, 0, 1};
        newForward = Vector3Normalize(newForward);

        Vector3 newX = Vector3CrossProduct(normal, newForward);
        if (Vector3Length(newX) < 0.001f) newX = {1, 0, 0};
        newX = Vector3Normalize(newX);

        Matrix mat = {0};
        mat.m0 = newX.x;      mat.m1 = newX.y;      mat.m2 = newX.z;      mat.m3 = 0.0f;
        mat.m4 = normal.x;    mat.m5 = normal.y;    mat.m6 = normal.z;    mat.m7 = 0.0f;
        mat.m8 = newForward.x; mat.m9 = newForward.y; mat.m10 = newForward.z; mat.m11 = 0.0f;
        mat.m12 = 0.0f; mat.m13 = 0.0f; mat.m14 = 0.0f; mat.m15 = 1.0f;

        float m8v = mat.m8;
        if (m8v > 1.0f) m8v = 1.0f;
        if (m8v < -1.0f) m8v = -1.0f;
        float yawPrincipal = asinf(m8v);
        float yawRad, pitchRad, rollRad;
        if (mat.m10 >= 0.0f) {
            yawRad = yawPrincipal;
            pitchRad = atan2f(-mat.m9, mat.m10);
            rollRad = atan2f(-mat.m4, mat.m0);
        } else {
            yawRad = (yawPrincipal >= 0.0f) ? (PI - yawPrincipal) : (-PI - yawPrincipal);
            pitchRad = atan2f(mat.m9, -mat.m10);
            rollRad = atan2f(mat.m4, -mat.m0);
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", pitchRad * RAD2DEG, yawRad * RAD2DEG, rollRad * RAD2DEG);
        for (Pin& p : node.outputs) {
            if (p.name == "Rotation") p.defaultValue = buf;
        }
    };
    NodeRegistry::Get().RegisterNode(alignToGround);
}
