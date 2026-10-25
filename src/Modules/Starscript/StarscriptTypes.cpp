#include "StarscriptTypes.h"
#include "Core/ObjectKindRegistry.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace Starscript {

std::string SType::ToString() const {
    switch (kind) {
        case Kind::Void: return "void";
        case Kind::Bool: return "bool";
        case Kind::Int: return "int";
        case Kind::Float: return "float";
        case Kind::String: return "string";
        case Kind::Vec3: return "Vec3";
        case Kind::Actor: return objectKind.empty() ? "Actor" : objectKind;
        case Kind::Component: return objectKind.empty() ? "Component" : objectKind;
        case Kind::Array: {
            SType e; e.kind = elem;
            return "Array[" + e.ToString() + "]";
        }
        case Kind::Any: return "any";
    }
    return "?";
}

SType MakeType(Kind kind) {
    SType t;
    t.kind = kind;
    return t;
}

SType MakeObjectType(Kind kind, const std::string& objectKind) {
    SType t;
    t.kind = kind;
    t.objectKind = objectKind;
    return t;
}

SType MakeArrayType(Kind elem) {
    SType t;
    t.kind = Kind::Array;
    t.elem = elem;
    return t;
}

static bool ParseElemKind(const std::string& name, Kind& out) {
    if (name == "bool") { out = Kind::Bool; return true; }
    if (name == "int") { out = Kind::Int; return true; }
    if (name == "float") { out = Kind::Float; return true; }
    if (name == "string") { out = Kind::String; return true; }
    if (name == "Vec3") { out = Kind::Vec3; return true; }
    return false;
}

bool ParseTypeName(const std::string& name, SType& out) {
    Kind k;
    if (ParseElemKind(name, k)) { out = MakeType(k); return true; }
    if (name == "Actor") { out = MakeObjectType(Kind::Actor, "Actor"); return true; }
    if (name.rfind("Array[", 0) == 0 && name.size() > 7 && name.back() == ']') {
        if (ParseElemKind(name.substr(6, name.size() - 7), k)) { out = MakeArrayType(k); return true; }
        return false;
    }
    return false;
}

bool IsAssignable(const SType& to, const SType& from) {
    if (to.kind == Kind::Any) return from.kind != Kind::Void;
    if (to.kind == Kind::Float && from.kind == Kind::Int) return true;
    if (to.kind != from.kind) return false;
    if (to.kind == Kind::Array) return to.elem == from.elem || to.elem == Kind::Void || from.elem == Kind::Void;
    if (to.kind == Kind::Actor || to.kind == Kind::Component) {
        if (to.objectKind.empty() || to.objectKind == from.objectKind) return true;
        return ObjectKindRegistry::Get().IsCompatible(from.objectKind, to.objectKind);
    }
    return true;
}

Value Value::Void() { return Value(); }
Value Value::Bool(bool v) { Value r; r.kind = Kind::Bool; r.b = v; return r; }
Value Value::Int(long long v) { Value r; r.kind = Kind::Int; r.i = v; return r; }
Value Value::Float(double v) { Value r; r.kind = Kind::Float; r.f = v; return r; }
Value Value::String(std::string v) { Value r; r.kind = Kind::String; r.s = std::move(v); return r; }
Value Value::Vec3(float x, float y, float z) { Value r; r.kind = Kind::Vec3; r.x = x; r.y = y; r.z = z; return r; }
Value Value::Object(Kind kind, unsigned id) { Value r; r.kind = kind; r.objectId = id; return r; }
Value Value::Array() { Value r; r.kind = Kind::Array; r.arr = std::make_shared<std::vector<Value>>(); return r; }

Value Value::Default(const SType& type) {
    switch (type.kind) {
        case Kind::Bool: return Bool(false);
        case Kind::Int: return Int(0);
        case Kind::Float: return Float(0.0);
        case Kind::String: return String("");
        case Kind::Vec3: return Vec3(0, 0, 0);
        case Kind::Actor:
        case Kind::Component: return Object(type.kind, 0);
        case Kind::Array: { Value v = Array(); return v; }
        default: return Void();
    }
}

static std::string FormatNumber(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

std::string ToDisplayString(const Value& v) {
    switch (v.kind) {
        case Kind::Void: return "void";
        case Kind::Bool: return v.b ? "true" : "false";
        case Kind::Int: return std::to_string(v.i);
        case Kind::Float: return FormatNumber(v.f);
        case Kind::String: return v.s;
        case Kind::Vec3: return "Vec3(" + FormatNumber(v.x) + ", " + FormatNumber(v.y) + ", " + FormatNumber(v.z) + ")";
        case Kind::Actor: return v.objectId ? "Actor#" + std::to_string(v.objectId) : "null";
        case Kind::Component: return v.objectId ? "Component#" + std::to_string(v.objectId) : "null";
        case Kind::Array: {
            std::string out = "[";
            if (v.arr) {
                for (size_t n = 0; n < v.arr->size(); n++) {
                    if (n) out += ", ";
                    out += ToDisplayString((*v.arr)[n]);
                }
            }
            return out + "]";
        }
        default: return "";
    }
}

std::string Serialize(const Value& v) {
    switch (v.kind) {
        case Kind::Bool: return v.b ? "true" : "false";
        case Kind::Int: return std::to_string(v.i);
        case Kind::Float: return FormatNumber(v.f);
        case Kind::String: return v.s;
        case Kind::Vec3: return FormatNumber(v.x) + "," + FormatNumber(v.y) + "," + FormatNumber(v.z);
        case Kind::Actor:
        case Kind::Component: return v.objectId ? std::to_string(v.objectId) : std::string();
        default: return std::string();
    }
}

Value Deserialize(const SType& type, const std::string& text) {
    switch (type.kind) {
        case Kind::Bool: return Value::Bool(text == "true" || text == "1");
        case Kind::Int: return Value::Int(text.empty() ? 0 : strtoll(text.c_str(), nullptr, 10));
        case Kind::Float: return Value::Float(text.empty() ? 0.0 : strtod(text.c_str(), nullptr));
        case Kind::String: return Value::String(text);
        case Kind::Vec3: {
            float x = 0, y = 0, z = 0;
            sscanf(text.c_str(), "%f,%f,%f", &x, &y, &z);
            return Value::Vec3(x, y, z);
        }
        case Kind::Actor:
        case Kind::Component: {
            size_t sep = text.find(':');
            std::string idText = sep == std::string::npos ? text : text.substr(sep + 1);
            unsigned id = idText.empty() ? 0u : (unsigned)strtoul(idText.c_str(), nullptr, 10);
            return Value::Object(type.kind, id);
        }
        default: return Value::Default(type);
    }
}

Value DeepCopy(const Value& v) {
    if (v.kind != Kind::Array || !v.arr) return v;
    Value copy = v;
    copy.arr = std::make_shared<std::vector<Value>>();
    for (const Value& e : *v.arr) copy.arr->push_back(DeepCopy(e));
    return copy;
}

}
