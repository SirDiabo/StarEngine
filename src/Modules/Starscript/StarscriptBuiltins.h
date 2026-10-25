#pragma once
#include "StarscriptTypes.h"
#include <functional>
#include <string>
#include <vector>

class Actor;
class Component;

namespace Starscript {

class Instance;

struct ScriptError {
    std::string message;
};

struct Runtime {
    Actor* owner = nullptr;
    Instance* instance = nullptr;
};

enum class RetRule { Fixed, Numeric, ElemType };

using BuiltinFn = Value (*)(Runtime& rt, std::vector<Value>& args);
using MethodFn = Value (*)(Runtime& rt, Value& self, std::vector<Value>& args);
using PropertyGetFn = Value (*)(Runtime& rt, const Value& self);
using PropertySetFn = void (*)(Runtime& rt, const Value& self, const Value& value);

struct BuiltinDef {
    const char* name;
    std::vector<SType> params;
    size_t minArgs;
    SType ret;
    RetRule rule;
    bool variadic;
    BuiltinFn fn;
};

struct MethodDef {
    Kind receiver;
    const char* name;
    std::vector<SType> params;
    size_t minArgs;
    SType ret;
    RetRule rule;
    bool elemParam;
    MethodFn fn;
};

struct PropertyDef {
    Kind receiver;
    const char* name;
    SType type;
    PropertyGetFn get;
    PropertySetFn set;
};

const std::vector<BuiltinDef>& AllBuiltins();
const std::vector<MethodDef>& AllMethods();
const std::vector<PropertyDef>& AllProperties();

const BuiltinDef* FindBuiltin(const std::string& name);
const MethodDef* FindMethod(Kind receiver, const std::string& name);
const PropertyDef* FindProperty(Kind receiver, const std::string& name);

Actor* ResolveActor(const Value& v);
Component* ResolveComponent(const Value& v);
Value MakeActorValue(Actor* actor);
Value MakeComponentValue(Component* component);

}
