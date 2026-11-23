#include "Nodes_Rotator.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "Core/RotationMath.h"
#include <cstdio>
#include <string>

namespace {

constexpr const char* kResult = "Return Value";

std::string FormatVec3(const Vector3& v) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g", v.x, v.y, v.z);
    return buf;
}

Vector3 ReadPin(const Node& node, const char* name) {
    Vector3 value{ 0.0f, 0.0f, 0.0f };
    for (const Pin& p : node.inputs) {
        if (p.name == name) { ReadVec3(p, value.x, value.y, value.z); break; }
    }
    return value;
}

void WriteResult(Node& node, const Vector3& value) {
    std::string text = FormatVec3(value);
    for (Pin& p : node.outputs) if (p.name == kResult) p.defaultValue = text;
}

const char* DefaultFor(const std::string& pin) {
    if (pin == "Right") return "-1,0,0";
    if (pin == "Up") return "0,1,0";
    return "0,0,1";
}

void RegisterDirection(const char* type, const char* label, Vector3 (*fn)(const Vector3&)) {
    NodeTemplate t;
    t.type = type;
    t.menuLabel = label;
    t.category = "Math";
    t.moduleName = "Stargraph";
    t.createPins = [](Node* node) {
        node->inputs.push_back({"In Rot", PinType::Rotator, true, false, 0, "0,0,0"});
        node->outputs.push_back({kResult, PinType::Vec3, false, true});
    };
    t.execute = [fn](Node& node, Stargraph&, ExecutionContext&) {
        WriteResult(node, fn(ReadPin(node, "In Rot")));
    };
    NodeRegistry::Get().RegisterNode(t);
}

struct MakeRotDef {
    const char* type;
    const char* label;
    const char* first;
    const char* second;
    Vector3 (*one)(Vector3);
    Vector3 (*two)(Vector3, const Vector3&);
};

void RegisterMakeRot(const MakeRotDef& def) {
    NodeTemplate t;
    t.type = def.type;
    t.menuLabel = def.label;
    t.category = "Math";
    t.moduleName = "Stargraph";
    std::string first = def.first;
    std::string second = def.second ? def.second : "";
    t.createPins = [first, second](Node* node) {
        node->inputs.push_back({first, PinType::Vec3, true, false, 0, DefaultFor(first)});
        if (!second.empty()) node->inputs.push_back({second, PinType::Vec3, true, false, 0, DefaultFor(second)});
        node->outputs.push_back({kResult, PinType::Rotator, false, true});
    };
    auto one = def.one;
    auto two = def.two;
    t.execute = [first, second, one, two](Node& node, Stargraph&, ExecutionContext&) {
        Vector3 a = ReadPin(node, first.c_str());
        if (second.empty()) WriteResult(node, one(a));
        else WriteResult(node, two(a, ReadPin(node, second.c_str())));
    };
    NodeRegistry::Get().RegisterNode(t);
}

}

void RegisterRotatorNodes() {
    NodeTemplate wrap;
    wrap.type = "Wrap_Rotator";
    wrap.menuLabel = "Wrap Rotator (-180 to 180)";
    wrap.category = "Math";
    wrap.moduleName = "Stargraph";
    wrap.createPins = [](Node* node) {
        node->inputs.push_back({"Rotator", PinType::Rotator, true, false, 0, "0,0,0"});
        node->outputs.push_back({kResult, PinType::Rotator, false, true});
    };
    wrap.execute = [](Node& node, Stargraph&, ExecutionContext&) {
        WriteResult(node, WrapRotationDegrees(ReadPin(node, "Rotator")));
    };
    NodeRegistry::Get().RegisterNode(wrap);

    RegisterDirection("Get_Forward_Vector", "Get Forward Vector", RotationForwardVector);
    RegisterDirection("Get_Right_Vector", "Get Right Vector", RotationRightVector);
    RegisterDirection("Get_Up_Vector", "Get Up Vector", RotationUpVector);

    static const MakeRotDef defs[] = {
        { "Make_Rot_From_X", "Make Rot From X", "Right", nullptr, MakeRotFromX, nullptr },
        { "Make_Rot_From_XY", "Make Rot From XY", "Right", "Up", nullptr, MakeRotFromXY },
        { "Make_Rot_From_XZ", "Make Rot From XZ", "Right", "Forward", nullptr, MakeRotFromXZ },
        { "Make_Rot_From_Y", "Make Rot From Y", "Up", nullptr, MakeRotFromY, nullptr },
        { "Make_Rot_From_YX", "Make Rot From YX", "Up", "Right", nullptr, MakeRotFromYX },
        { "Make_Rot_From_YZ", "Make Rot From YZ", "Up", "Forward", nullptr, MakeRotFromYZ },
        { "Make_Rot_From_Z", "Make Rot From Z", "Forward", nullptr, MakeRotFromZ, nullptr },
        { "Make_Rot_From_ZX", "Make Rot From ZX", "Forward", "Right", nullptr, MakeRotFromZX },
        { "Make_Rot_From_ZY", "Make Rot From ZY", "Forward", "Up", nullptr, MakeRotFromZY },
    };
    for (const MakeRotDef& def : defs) RegisterMakeRot(def);
}
