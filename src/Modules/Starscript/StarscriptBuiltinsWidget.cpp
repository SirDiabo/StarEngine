#include "StarscriptBuiltins.h"
#include "StarscriptRuntime.h"
#include "Runtime/Actor.h"
#include "Runtime/WidgetBehavior.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Core/ObjectRefRegistry.h"
#include "Core/ObjectKindRegistry.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/Widgets/WidgetObjectElement.h"
#include "raylib.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Starscript {

WidgetElement* ResolveElement(const Value& v) {
    if (v.kind != Kind::Element || v.objectId == 0) return nullptr;
    return dynamic_cast<WidgetElement*>(ObjectRefRegistry::Get().Resolve(v.objectId));
}

Value MakeElementValue(WidgetElement* element) {
    return Value::Object(Kind::Element, element ? element->GetObjectId() : 0);
}

namespace {

SType T(Kind k) { return MakeType(k); }
SType ElementT() { return MakeObjectType(Kind::Element, "WidgetElement"); }
SType ActorT() { return MakeObjectType(Kind::Actor, "Actor"); }
SType ObjectElementT() { return MakeObjectType(Kind::Element, "WidgetObjectElement"); }

Widget* RequireWidget(Runtime& rt, const char* what) {
    Widget* widget = rt.instance ? rt.instance->GetWidget() : nullptr;
    if (!widget) throw ScriptError{ std::string(what) + ": this is only available in a widget script" };
    return widget;
}

WidgetElement* RequireElement(const Value& v, const char* what) {
    WidgetElement* e = ResolveElement(v);
    if (!e) throw ScriptError{ std::string(what) + ": the element is not valid" };
    return e;
}

WidgetObjectElement* RequireObjectElement(const Value& v, const char* what) {
    WidgetObjectElement* e = dynamic_cast<WidgetObjectElement*>(RequireElement(v, what));
    if (!e) throw ScriptError{ std::string(what) + ": this element is not a WidgetObjectElement" };
    return e;
}

Value V2(const Vector2& v) { return Value::Vec3(v.x, v.y, 0.0f); }
Vector2 ToV2(const Value& v) { return { v.x, v.y }; }

std::string SerializeCallArg(const Value& v) {
    if (v.kind == Kind::Actor || v.kind == Kind::Component || v.kind == Kind::Element) {
        if (v.objectId == 0) return std::string();
        if (Object* obj = ObjectRefRegistry::Get().Resolve(v.objectId))
            return ObjectRefRegistry::Encode(obj->GetObjectKind(), v.objectId);
        return std::string();
    }
    return Serialize(v);
}

WidgetBehavior::CallParams PositionalParams(const std::vector<Value>& a, size_t first) {
    WidgetBehavior::CallParams params;
    for (size_t n = first; n < a.size(); n++) params.emplace_back("p" + std::to_string(n - first), SerializeCallArg(a[n]));
    return params;
}

WidgetBehavior* ChildBehavior(WidgetObjectElement* target, const char* what) {
    Widget* child = target->GetChildWidget();
    if (!child) throw ScriptError{ std::string(what) + ": the widget object has no widget assigned" };
    WidgetBehavior* behavior = child->GetBehavior();
    if (!behavior) throw ScriptError{ std::string(what) + ": the child widget has no script or graph" };
    return behavior;
}

Value Fn_get_element(Runtime& rt, std::vector<Value>& a) {
    RequireWidget(rt, "get_element");
    WidgetElement* found = rt.instance->FindElementByName(a[0].s);
    if (!found) throw ScriptError{ "get_element: no element named '" + a[0].s + "' in this widget" };
    return MakeElementValue(found);
}

Value Fn_get_parent_actor(Runtime& rt, std::vector<Value>&) {
    Widget* widget = RequireWidget(rt, "get_parent_actor");
    Actor* owner = widget->owningComponent ? widget->owningComponent->GetOwningActor() : nullptr;
    return MakeActorValue(owner);
}

Value Fn_set_widget(Runtime& rt, std::vector<Value>& a) {
    Widget* widget = RequireWidget(rt, "set_widget");
    if (!widget->owningComponent) throw ScriptError{ "set_widget: this widget is not shown by a WidgetComponent" };
    widget->owningComponent->widgetAssetPath = a[0].s;
    return Value::Void();
}

Value Fn_widget_scale(Runtime& rt, std::vector<Value>&) {
    return Value::Float(RequireWidget(rt, "widget_scale")->uiScale);
}

Value Fn_set_widget_scale(Runtime& rt, std::vector<Value>& a) {
    RequireWidget(rt, "set_widget_scale")->uiScale = (float)a[0].AsNumber();
    return Value::Void();
}

Value M_element_is_valid(Runtime&, Value& s, std::vector<Value>&) { return Value::Bool(ResolveElement(s) != nullptr); }

Value M_element_get_child(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetElement* e = RequireElement(s, "get_child");
    const std::vector<WidgetElement*>& kids = e->GetAuthoredChildren();
    if (a[0].i < 0 || a[0].i >= (long long)kids.size()) throw ScriptError{ "get_child: index " + std::to_string(a[0].i) + " is out of range" };
    return MakeElementValue(kids[(size_t)a[0].i]);
}

Value M_element_clear_children(Runtime&, Value& s, std::vector<Value>&) {
    RequireElement(s, "clear_children")->ClearChildren();
    return Value::Void();
}

Value M_element_select(Runtime&, Value& s, std::vector<Value>&) {
    WidgetElement* e = RequireElement(s, "select");
    if (e->owningWidget) e->owningWidget->SetSelection(e);
    return Value::Void();
}

Value M_element_add_widget(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetElement* container = RequireElement(s, "add_widget");
    if (a[0].s.empty()) throw ScriptError{ "add_widget: the widget asset path is empty" };
    WidgetObjectElement* object = new WidgetObjectElement();
    if (a.size() > 1 && !a[1].s.empty()) object->elementId = a[1].s;
    container->AddChild(object);
    object->SetWidgetAsset(a[0].s);
    return MakeElementValue(object);
}

Value M_element_set_widget_asset(Runtime&, Value& s, std::vector<Value>& a) {
    RequireObjectElement(s, "set_widget_asset")->SetWidgetAsset(a[0].s);
    return Value::Void();
}

Value M_element_call_event(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetObjectElement* target = RequireObjectElement(s, "call_event");
    ChildBehavior(target, "call_event")->CallWidgetEvent(target->GetChildWidget(), a[0].s, PositionalParams(a, 1));
    return Value::Void();
}

Value M_element_call_function(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetObjectElement* target = RequireObjectElement(s, "call_function");
    std::string result;
    if (!ChildBehavior(target, "call_function")->CallWidgetFunction(target->GetChildWidget(), a[0].s, PositionalParams(a, 1), result))
        throw ScriptError{ "call_function: the child widget has no function '" + a[0].s + "'" };
    return Value::String(result);
}

Value M_element_set_variable(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetObjectElement* target = RequireObjectElement(s, "set_variable");
    if (!ChildBehavior(target, "set_variable")->SetWidgetVariable(target->GetChildWidget(), a[0].s, SerializeCallArg(a[1])))
        throw ScriptError{ "set_variable: the child widget has no variable '" + a[0].s + "'" };
    return Value::Void();
}

Value M_element_get_variable(Runtime&, Value& s, std::vector<Value>& a) {
    WidgetObjectElement* target = RequireObjectElement(s, "get_variable");
    std::string value;
    if (!ChildBehavior(target, "get_variable")->GetWidgetVariable(target->GetChildWidget(), a[0].s, value))
        throw ScriptError{ "get_variable: the child widget has no variable '" + a[0].s + "'" };
    return Value::String(value);
}

Value P_el_name(Runtime&, const Value& s) { return Value::String(RequireElement(s, "name")->elementId); }
Value P_el_class(Runtime&, const Value& s) { return Value::String(RequireElement(s, "class_name")->GetClassName()); }

Value P_el_get_visible(Runtime&, const Value& s) { return Value::Bool(RequireElement(s, "visible")->visible); }
void P_el_set_visible(Runtime&, const Value& s, const Value& v) { RequireElement(s, "visible")->visible = v.b; }

Value P_el_get_disabled(Runtime&, const Value& s) { return Value::Bool(RequireElement(s, "disabled")->disabled); }
void P_el_set_disabled(Runtime&, const Value& s, const Value& v) {
    WidgetElement* e = RequireElement(s, "disabled");
    e->disabled = v.b;
    if (!v.b && e->interactionState == WidgetInteractionState::Disabled) e->interactionState = WidgetInteractionState::Default;
}

Value P_el_get_offset(Runtime&, const Value& s) { return V2(RequireElement(s, "offset")->offset); }
void P_el_set_offset(Runtime&, const Value& s, const Value& v) { RequireElement(s, "offset")->offset = ToV2(v); }
Value P_el_get_size(Runtime&, const Value& s) { return V2(RequireElement(s, "size")->size); }
void P_el_set_size(Runtime&, const Value& s, const Value& v) { RequireElement(s, "size")->size = ToV2(v); }
Value P_el_get_anchor(Runtime&, const Value& s) { return V2(RequireElement(s, "anchor")->anchor); }
void P_el_set_anchor(Runtime&, const Value& s, const Value& v) { RequireElement(s, "anchor")->anchor = ToV2(v); }
Value P_el_get_pivot(Runtime&, const Value& s) { return V2(RequireElement(s, "pivot")->pivot); }
void P_el_set_pivot(Runtime&, const Value& s, const Value& v) { RequireElement(s, "pivot")->pivot = ToV2(v); }

Value P_el_screen_position(Runtime&, const Value& s) {
    const Rectangle& r = RequireElement(s, "screen_position")->lastResolvedRect;
    return Value::Vec3(r.x, r.y, 0.0f);
}
Value P_el_screen_size(Runtime&, const Value& s) {
    const Rectangle& r = RequireElement(s, "screen_size")->lastResolvedRect;
    return Value::Vec3(r.width, r.height, 0.0f);
}

Value P_el_parent(Runtime&, const Value& s) { return MakeElementValue(RequireElement(s, "parent")->parent); }
Value P_el_child_count(Runtime&, const Value& s) { return Value::Int((long long)RequireElement(s, "child_count")->GetAuthoredChildren().size()); }

Value P_el_hovered(Runtime&, const Value& s) { return Value::Bool(RequireElement(s, "hovered")->interactionState == WidgetInteractionState::Hovered); }
Value P_el_pressed(Runtime&, const Value& s) { return Value::Bool(RequireElement(s, "pressed")->interactionState == WidgetInteractionState::Pressed); }
Value P_el_selected(Runtime&, const Value& s) {
    WidgetElement* e = RequireElement(s, "selected");
    return Value::Bool(e->owningWidget && e->owningWidget->currentSelection == e);
}

}

const std::vector<BuiltinDef>& WidgetBuiltins() {
    static const std::vector<BuiltinDef> table = {
        { "get_element", { T(Kind::String) }, 1, ElementT(), RetRule::Fixed, false, Fn_get_element, 0, ScopeWidget },
        { "get_parent_actor", {}, 0, ActorT(), RetRule::Fixed, false, Fn_get_parent_actor, 0, ScopeWidget },
        { "set_widget", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_widget, 0, ScopeWidget },
        { "widget_scale", {}, 0, T(Kind::Float), RetRule::Fixed, false, Fn_widget_scale, 0, ScopeWidget },
        { "set_widget_scale", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_widget_scale, 0, ScopeWidget },
    };
    return table;
}

const std::vector<MethodDef>& WidgetMethods() {
    static const std::vector<MethodDef> table = {
        { Kind::Element, "is_valid", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_element_is_valid },
        { Kind::Element, "get_child", { T(Kind::Int) }, 1, ElementT(), RetRule::Fixed, false, M_element_get_child },
        { Kind::Element, "clear_children", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_element_clear_children },
        { Kind::Element, "select", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_element_select },
        { Kind::Element, "add_widget", { T(Kind::String), T(Kind::String) }, 1, ObjectElementT(), RetRule::Fixed, false, M_element_add_widget },
        { Kind::Element, "set_widget_asset", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, M_element_set_widget_asset, "WidgetObjectElement" },
        { Kind::Element, "call_event", { T(Kind::String), T(Kind::Any), T(Kind::Any), T(Kind::Any), T(Kind::Any) }, 1, T(Kind::Void), RetRule::Fixed, false, M_element_call_event, "WidgetObjectElement" },
        { Kind::Element, "call_function", { T(Kind::String), T(Kind::Any), T(Kind::Any), T(Kind::Any), T(Kind::Any) }, 1, T(Kind::String), RetRule::Fixed, false, M_element_call_function, "WidgetObjectElement" },
        { Kind::Element, "set_variable", { T(Kind::String), T(Kind::Any) }, 2, T(Kind::Void), RetRule::Fixed, false, M_element_set_variable, "WidgetObjectElement" },
        { Kind::Element, "get_variable", { T(Kind::String) }, 1, T(Kind::String), RetRule::Fixed, false, M_element_get_variable, "WidgetObjectElement" },
    };
    return table;
}

const std::vector<PropertyDef>& WidgetProperties() {
    static const std::vector<PropertyDef> table = {
        { Kind::Element, "name", T(Kind::String), P_el_name, nullptr },
        { Kind::Element, "class_name", T(Kind::String), P_el_class, nullptr },
        { Kind::Element, "visible", T(Kind::Bool), P_el_get_visible, P_el_set_visible },
        { Kind::Element, "disabled", T(Kind::Bool), P_el_get_disabled, P_el_set_disabled },
        { Kind::Element, "offset", T(Kind::Vec3), P_el_get_offset, P_el_set_offset },
        { Kind::Element, "size", T(Kind::Vec3), P_el_get_size, P_el_set_size },
        { Kind::Element, "anchor", T(Kind::Vec3), P_el_get_anchor, P_el_set_anchor },
        { Kind::Element, "pivot", T(Kind::Vec3), P_el_get_pivot, P_el_set_pivot },
        { Kind::Element, "screen_position", T(Kind::Vec3), P_el_screen_position, nullptr },
        { Kind::Element, "screen_size", T(Kind::Vec3), P_el_screen_size, nullptr },
        { Kind::Element, "parent", ElementT(), P_el_parent, nullptr },
        { Kind::Element, "child_count", T(Kind::Int), P_el_child_count, nullptr },
        { Kind::Element, "hovered", T(Kind::Bool), P_el_hovered, nullptr },
        { Kind::Element, "pressed", T(Kind::Bool), P_el_pressed, nullptr },
        { Kind::Element, "selected", T(Kind::Bool), P_el_selected, nullptr },
    };
    return table;
}

namespace {

std::string NormalizeKey(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == ' ' || c == '_') continue;
        out += (char)tolower((unsigned char)c);
    }
    return out;
}

bool FieldAppliesTo(const WidgetElementFieldDesc& field, const std::string& elementClass, bool exactOnly) {
    if (!field.elementType) return false;
    if (elementClass == field.elementType) return true;
    if (exactOnly) return false;
    return ObjectKindRegistry::Get().IsCompatible(elementClass, field.elementType);
}

const WidgetElementFieldDesc* FindFieldByKey(const std::string& elementClass, const std::string& key) {
    for (int pass = 0; pass < 2; pass++) {
        for (const WidgetElementFieldDesc& f : GetWidgetElementFields()) {
            if (f.key && key == f.key && FieldAppliesTo(f, elementClass, pass == 0)) return &f;
        }
    }
    return nullptr;
}

}

std::string ElementFieldIdentifier(const WidgetElementFieldDesc& field) {
    std::string key = field.key ? field.key : "";
    std::string out;
    for (size_t n = 0; n < key.size(); n++) {
        char c = key[n];
        if (isupper((unsigned char)c)) {
            if (n > 0 && !isupper((unsigned char)key[n - 1])) out += '_';
            out += (char)tolower((unsigned char)c);
        } else {
            out += c;
        }
    }
    return out;
}

const WidgetElementFieldDesc* FindElementField(const std::string& elementClass, const std::string& identifier) {
    std::string wanted = NormalizeKey(identifier);
    for (int pass = 0; pass < 2; pass++) {
        for (const WidgetElementFieldDesc& f : GetWidgetElementFields()) {
            if (!f.key || NormalizeKey(f.key) != wanted) continue;
            if (FieldAppliesTo(f, elementClass, pass == 0)) return &f;
        }
    }
    return nullptr;
}

SType ElementFieldScriptType(const WidgetElementFieldDesc& field) {
    switch (field.kind) {
        case WidgetFieldKind::Float: return MakeType(Kind::Float);
        case WidgetFieldKind::Int: return MakeType(Kind::Int);
        case WidgetFieldKind::Bool: return MakeType(Kind::Bool);
        default: return MakeType(Kind::String);
    }
}

Value ReadElementField(const Value& element, const std::string& fieldKey) {
    WidgetElement* e = RequireElement(element, "element field");
    const WidgetElementFieldDesc* field = FindFieldByKey(e->GetClassName(), fieldKey);
    if (!field || !field->getValue) throw ScriptError{ "This element field cannot be read" };
    std::string text = field->getValue(*e);
    switch (field->kind) {
        case WidgetFieldKind::Float: return Value::Float(text.empty() ? 0.0 : strtod(text.c_str(), nullptr));
        case WidgetFieldKind::Int: return Value::Int(text.empty() ? 0 : strtoll(text.c_str(), nullptr, 10));
        case WidgetFieldKind::Bool: return Value::Bool(text == "true" || text == "1");
        default: return Value::String(text);
    }
}

void WriteElementField(const Value& element, const std::string& fieldKey, const Value& value) {
    WidgetElement* e = RequireElement(element, "element field");
    const WidgetElementFieldDesc* field = FindFieldByKey(e->GetClassName(), fieldKey);
    if (!field || !field->setValue) throw ScriptError{ "This element field cannot be written" };
    switch (field->kind) {
        case WidgetFieldKind::Float: { char buf[64]; snprintf(buf, sizeof(buf), "%.9g", value.AsNumber()); field->setValue(*e, buf); break; }
        case WidgetFieldKind::Int: field->setValue(*e, std::to_string(value.kind == Kind::Int ? value.i : (long long)value.AsNumber())); break;
        case WidgetFieldKind::Bool: field->setValue(*e, value.b ? "true" : "false"); break;
        default: field->setValue(*e, value.s); break;
    }
}

}
