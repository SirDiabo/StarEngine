#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Starscript {

enum class Kind { Void, Bool, Int, Float, String, Vec3, Actor, Component, Array, Hit, Element, Any };

struct SType {
    Kind kind = Kind::Void;
    std::string objectKind;
    Kind elem = Kind::Void;

    bool operator==(const SType& other) const {
        return kind == other.kind && objectKind == other.objectKind && elem == other.elem;
    }
    bool operator!=(const SType& other) const { return !(*this == other); }

    bool IsNumeric() const { return kind == Kind::Int || kind == Kind::Float; }
    std::string ToString() const;
};

SType MakeType(Kind kind);
SType MakeObjectType(Kind kind, const std::string& objectKind);
SType MakeArrayType(Kind elem);
SType ElementType(Kind elem);
bool ParseTypeName(const std::string& name, SType& out);
bool IsAssignable(const SType& to, const SType& from);

struct Value {
    Kind kind = Kind::Void;
    bool b = false;
    long long i = 0;
    double f = 0.0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    std::string s;
    unsigned objectId = 0;
    std::shared_ptr<std::vector<Value>> arr;

    static Value Void();
    static Value Bool(bool v);
    static Value Int(long long v);
    static Value Float(double v);
    static Value String(std::string v);
    static Value Vec3(float x, float y, float z);
    static Value Object(Kind kind, unsigned id);
    static Value Array();
    static Value Default(const SType& type);

    double AsNumber() const { return kind == Kind::Int ? (double)i : f; }
};

std::string ToDisplayString(const Value& v);
std::string Serialize(const Value& v);
Value Deserialize(const SType& type, const std::string& text);
Value DeepCopy(const Value& v);

}
