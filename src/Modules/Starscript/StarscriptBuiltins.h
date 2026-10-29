#pragma once
#include "StarscriptTypes.h"
#include <functional>
#include <string>
#include <vector>

class Actor;
class Component;
class WidgetElement;
struct ComponentFieldDesc;
struct WidgetElementFieldDesc;

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

enum BuiltinScope { ScopeAny = 0, ScopeWidget = 1, ScopeActor = 2 };

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
    int asyncKind = 0;
    int scope = 0;
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
    const char* componentType = "";
};

struct PropertyDef {
    Kind receiver;
    const char* name;
    SType type;
    PropertyGetFn get;
    PropertySetFn set;
    const char* componentType = "";
};

const std::vector<BuiltinDef>& ExtraBuiltins();
const std::vector<MethodDef>& ExtraMethods();
const std::vector<PropertyDef>& ExtraProperties();

const std::vector<MethodDef>& ComponentMethods();
const std::vector<BuiltinDef>& WidgetBuiltins();
const std::vector<MethodDef>& WidgetMethods();
const std::vector<PropertyDef>& WidgetProperties();

const std::vector<BuiltinDef>& AllBuiltins();
const std::vector<MethodDef>& AllMethods();
const std::vector<PropertyDef>& AllProperties();

const BuiltinDef* FindBuiltin(const std::string& name);
const MethodDef* FindMethod(Kind receiver, const std::string& name, const std::string& objectKind = std::string());
const PropertyDef* FindProperty(Kind receiver, const std::string& name, const std::string& objectKind = std::string());

const ComponentFieldDesc* FindComponentField(const std::string& componentType, const std::string& identifier);
SType ComponentFieldScriptType(const ComponentFieldDesc& field);
std::string ComponentFieldIdentifier(const ComponentFieldDesc& field);
Value ReadComponentField(const Value& component, const std::string& fieldKey);
void WriteComponentField(const Value& component, const std::string& fieldKey, const Value& value);

const WidgetElementFieldDesc* FindElementField(const std::string& elementClass, const std::string& identifier);
SType ElementFieldScriptType(const WidgetElementFieldDesc& field);
std::string ElementFieldIdentifier(const WidgetElementFieldDesc& field);
Value ReadElementField(const Value& element, const std::string& fieldKey);
void WriteElementField(const Value& element, const std::string& fieldKey, const Value& value);

WidgetElement* ResolveElement(const Value& v);
Value MakeElementValue(WidgetElement* element);

Actor* ResolveActor(const Value& v);
Component* ResolveComponent(const Value& v);
Value MakeActorValue(Actor* actor);
Value MakeComponentValue(Component* component);

}
