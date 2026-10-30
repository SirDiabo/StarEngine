#include "StarscriptBuiltins.h"
#include "StarscriptRuntime.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/SaveGame.h"
#include "Runtime/AudioCore.h"
#include "Core/AudioBusRegistry.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Core/InputKeyNames.h"
#include "Core/ObjectKindRegistry.h"
#include "Core/SaveFileFormat.h"
#include "Core/SavePaths.h"
#include "Core/SaveSlots.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace Starscript {

namespace {

SType T(Kind k) { return MakeType(k); }
SType CompT() { return MakeObjectType(Kind::Component, "Component"); }
Value V3(const Vector3& v) { return Value::Vec3(v.x, v.y, v.z); }
Vector3 ToV3(const Value& v) { return { v.x, v.y, v.z }; }

Instance& RequireInstance(Runtime& rt) {
    if (!rt.instance) throw ScriptError{ "This function needs a running script" };
    return *rt.instance;
}

Actor* RequireOwner(Runtime& rt, const char* what) {
    if (!rt.owner) throw ScriptError{ std::string(what) + ": there is no owning actor" };
    return rt.owner;
}

Actor* RequireActorValue(const Value& v, const char* what) {
    Actor* a = ResolveActor(v);
    if (!a) throw ScriptError{ std::string(what) + ": the actor is not valid" };
    return a;
}

Component* RequireComp(const Value& v, const char* what) {
    Component* c = ResolveComponent(v);
    if (!c) throw ScriptError{ std::string(what) + ": the component is not valid" };
    return c;
}

std::string VariableKey(Actor* actor, const std::string& name) { return actor->GetName() + "." + name; }

Value Fn_delta_time(Runtime&, std::vector<Value>&) { return Value::Float(GetFrameTime()); }
Value Fn_delta_scale(Runtime&, std::vector<Value>& a) { return Value::Float((a.empty() ? 60.0 : a[0].AsNumber()) * GetFrameTime()); }
Value Fn_fps(Runtime&, std::vector<Value>&) { return Value::Int(GetFPS()); }
Value Fn_set_target_fps(Runtime&, std::vector<Value>& a) { SetTargetFPS((int)a[0].i); return Value::Void(); }
Value Fn_monitor_size(Runtime&, std::vector<Value>&) { int m = GetCurrentMonitor(); return Value::Vec3((float)GetMonitorWidth(m), (float)GetMonitorHeight(m), 0.0f); }
Value Fn_mouse_position(Runtime&, std::vector<Value>&) { Vector2 p = GetMousePosition(); return Value::Vec3(p.x, p.y, 0.0f); }
Value Fn_mouse_delta(Runtime&, std::vector<Value>&) { Vector2 p = GetMouseDelta(); return Value::Vec3(p.x, p.y, 0.0f); }
Value Fn_key_pressed(Runtime&, std::vector<Value>&) {
    int key = GetKeyPressed();
    return Value::String(key != 0 ? InputKeyCodeToName(key) : std::string());
}
Value Fn_char_pressed(Runtime&, std::vector<Value>&) {
    int cp = GetCharPressed();
    if (cp <= 0) return Value::String("");
    int size = 0;
    const char* utf8 = CodepointToUTF8(cp, &size);
    return Value::String(std::string(utf8, (size_t)size));
}

Value Fn_set_window_title(Runtime&, std::vector<Value>& a) { SetWindowTitle(a[0].s.c_str()); return Value::Void(); }
Value Fn_toggle_fullscreen(Runtime&, std::vector<Value>&) { ToggleFullscreen(); return Value::Void(); }
Value Fn_maximize_window(Runtime&, std::vector<Value>&) { MaximizeWindow(); return Value::Void(); }
Value Fn_minimize_window(Runtime&, std::vector<Value>&) { MinimizeWindow(); return Value::Void(); }
Value Fn_is_window_minimized(Runtime&, std::vector<Value>&) { return Value::Bool(IsWindowMinimized()); }
Value Fn_is_window_maximized(Runtime&, std::vector<Value>&) { return Value::Bool(IsWindowMaximized()); }
Value Fn_is_window_focused(Runtime&, std::vector<Value>&) { return Value::Bool(IsWindowFocused()); }
Value Fn_set_window_opacity(Runtime&, std::vector<Value>& a) { SetWindowOpacity((float)a[0].AsNumber()); return Value::Void(); }
Value Fn_window_position(Runtime&, std::vector<Value>&) { Vector2 p = GetWindowPosition(); return Value::Vec3(p.x, p.y, 0.0f); }
Value Fn_set_window_position(Runtime&, std::vector<Value>& a) { SetWindowPosition((int)a[0].x, (int)a[0].y); return Value::Void(); }
Value Fn_window_size(Runtime&, std::vector<Value>&) { return Value::Vec3((float)GetScreenWidth(), (float)GetScreenHeight(), 0.0f); }
Value Fn_set_window_size(Runtime&, std::vector<Value>& a) { SetWindowSize((int)a[0].x, (int)a[0].y); return Value::Void(); }
Value Fn_take_screenshot(Runtime&, std::vector<Value>& a) { TakeScreenshot(a[0].s.c_str()); return Value::Void(); }
Value Fn_open_url(Runtime&, std::vector<Value>& a) { OpenURL(a[0].s.c_str()); return Value::Void(); }
Value Fn_get_clipboard(Runtime&, std::vector<Value>&) { const char* t = GetClipboardText(); return Value::String(t ? t : ""); }
Value Fn_set_clipboard(Runtime&, std::vector<Value>& a) { SetClipboardText(a[0].s.c_str()); return Value::Void(); }

Value Fn_file_exists(Runtime&, std::vector<Value>& a) { return Value::Bool(FileExists(a[0].s.c_str())); }
Value Fn_directory_exists(Runtime&, std::vector<Value>& a) { return Value::Bool(DirectoryExists(a[0].s.c_str())); }
Value Fn_load_text_file(Runtime&, std::vector<Value>& a) {
    char* text = LoadFileText(a[0].s.c_str());
    if (!text) return Value::String("");
    Value v = Value::String(text);
    UnloadFileText(text);
    return v;
}
Value Fn_save_text_file(Runtime&, std::vector<Value>& a) { return Value::Bool(SaveFileText(a[0].s.c_str(), const_cast<char*>(a[1].s.c_str()))); }
Value Fn_working_directory(Runtime&, std::vector<Value>&) { return Value::String(GetWorkingDirectory()); }
Value Fn_change_directory(Runtime&, std::vector<Value>& a) { return Value::Bool(ChangeDirectory(a[0].s.c_str())); }

float InterpStep(float value, float target, float delta, float speed, const std::string& method) {
    float diff = target - value;
    if (method == "Linear") {
        float step = fabsf(speed) * delta;
        if (fabsf(diff) <= step) return target;
        return value + (diff > 0.0f ? step : -step);
    }
    float t = 1.0f - expf(-fabsf(speed) * delta);
    if (t > 1.0f) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    return value + diff * t;
}

Value Fn_interp_to(Runtime&, std::vector<Value>& a) {
    std::string method = a.size() > 4 ? a[4].s : std::string("Exponential");
    return Value::Float(InterpStep((float)a[0].AsNumber(), (float)a[1].AsNumber(), (float)a[2].AsNumber(), (float)a[3].AsNumber(), method));
}

Value Fn_vec3_interp_to(Runtime&, std::vector<Value>& a) {
    Vector3 value = ToV3(a[0]), target = ToV3(a[1]);
    float delta = (float)a[2].AsNumber(), speed = (float)a[3].AsNumber();
    std::string method = a.size() > 4 ? a[4].s : std::string("Exponential");
    if (method == "Linear") {
        Vector3 diff = Vector3Subtract(target, value);
        float dist = Vector3Length(diff);
        float step = fabsf(speed) * delta;
        if (dist <= step || dist < 1e-6f) return V3(target);
        return V3(Vector3Add(value, Vector3Scale(diff, step / dist)));
    }
    return Value::Vec3(InterpStep(value.x, target.x, delta, speed, method), InterpStep(value.y, target.y, delta, speed, method),
                       InterpStep(value.z, target.z, delta, speed, method));
}

Value Fn_fmod(Runtime&, std::vector<Value>& a) { double b = a[1].AsNumber(); return Value::Float(b != 0.0 ? std::fmod(a[0].AsNumber(), b) : 0.0); }
Value Fn_truncate(Runtime&, std::vector<Value>& a) { return Value::Int((long long)std::trunc(a[0].AsNumber())); }
Value Fn_random_bool(Runtime&, std::vector<Value>&) { return Value::Bool((rand() & 1) != 0); }
Value Fn_vec3_reflect(Runtime&, std::vector<Value>& a) { return V3(Vector3Reflect(ToV3(a[0]), ToV3(a[1]))); }
Value Fn_to_vec3(Runtime&, std::vector<Value>& a) { float x = 0, y = 0, z = 0; sscanf(a[0].s.c_str(), "%f,%f,%f", &x, &y, &z); return Value::Vec3(x, y, z); }

bool ClassMatches(Actor* actor, const std::string& name) { return ObjectKindRegistry::Get().IsCompatible(actor->GetObjectKind(), name); }
Value Fn_is_class(Runtime&, std::vector<Value>& a) {
    Actor* actor = ResolveActor(a[0]);
    return Value::Bool(actor && ClassMatches(actor, a[1].s));
}
Value M_actor_is_class(Runtime&, Value& s, std::vector<Value>& a) {
    Actor* actor = ResolveActor(s);
    return Value::Bool(actor && ClassMatches(actor, a[0].s));
}
Value M_actor_get_component(Runtime&, Value& s, std::vector<Value>& a) {
    Actor* actor = RequireActorValue(s, "get_component");
    return MakeComponentValue(actor->FindComponentByName(a[0].s));
}
Value P_actor_class_name(Runtime&, const Value& s) { return Value::String(RequireActorValue(s, "class_name")->GetObjectKind()); }

Value P_comp_world_position(Runtime&, const Value& s) {
    Component* c = RequireComp(s, "world_position");
    Actor* owner = c->GetOwningActor();
    Vector3 p{ 0, 0, 0 }, r{ 0, 0, 0 }, sc{ 1, 1, 1 };
    if (owner) owner->GetComponentWorldTransform(c, p, r, sc);
    return V3(p);
}
void P_comp_set_world_position(Runtime&, const Value& s, const Value& v) {
    Component* c = RequireComp(s, "world_position");
    if (Actor* owner = c->GetOwningActor()) owner->SetComponentWorldPosition(c, ToV3(v));
}
Value P_comp_world_rotation(Runtime&, const Value& s) {
    Component* c = RequireComp(s, "world_rotation");
    Actor* owner = c->GetOwningActor();
    Vector3 p{ 0, 0, 0 }, r{ 0, 0, 0 }, sc{ 1, 1, 1 };
    if (owner) owner->GetComponentWorldTransform(c, p, r, sc);
    return V3(r);
}
void P_comp_set_world_rotation(Runtime&, const Value& s, const Value& v) {
    Component* c = RequireComp(s, "world_rotation");
    if (Actor* owner = c->GetOwningActor()) owner->SetComponentWorldRotation(c, ToV3(v));
}
Value P_comp_world_scale(Runtime&, const Value& s) {
    Component* c = RequireComp(s, "world_scale");
    Actor* owner = c->GetOwningActor();
    Vector3 p{ 0, 0, 0 }, r{ 0, 0, 0 }, sc{ 1, 1, 1 };
    if (owner) owner->GetComponentWorldTransform(c, p, r, sc);
    return V3(sc);
}
void P_comp_set_world_scale(Runtime&, const Value& s, const Value& v) {
    Component* c = RequireComp(s, "world_scale");
    if (Actor* owner = c->GetOwningActor()) owner->SetComponentWorldScale(c, ToV3(v));
}

Value Fn_load_level(Runtime&, std::vector<Value>& a) { if (!a[0].s.empty()) World::Get().RequestLoadLevel(a[0].s); return Value::Void(); }
Value Fn_has_authority(Runtime&, std::vector<Value>&) { return Value::Bool(NetworkBackendRegistry::Get().HasAuthority()); }

Value Fn_save_all(Runtime& rt, std::vector<Value>&) { SaveGame::SaveActor(RequireOwner(rt, "save_all")); return Value::Void(); }
Value Fn_load_all(Runtime& rt, std::vector<Value>&) { SaveGame::LoadActor(RequireOwner(rt, "load_all")); return Value::Void(); }
Value Fn_save_file(Runtime& rt, std::vector<Value>& a) { SaveGame::SaveActor(RequireOwner(rt, "save_file"), a[0].s); return Value::Void(); }
Value Fn_load_file(Runtime& rt, std::vector<Value>& a) { SaveGame::LoadActor(RequireOwner(rt, "load_file"), a[0].s); return Value::Void(); }
Value Fn_save_game(Runtime&, std::vector<Value>&) { SaveGame::SaveWorld(); return Value::Void(); }
Value Fn_load_game(Runtime&, std::vector<Value>&) { SaveGame::LoadWorld(); return Value::Void(); }

Value Fn_save_variable(Runtime& rt, std::vector<Value>& a) {
    Instance& inst = RequireInstance(rt);
    Actor* owner = RequireOwner(rt, "save_variable");
    int idx = inst.FindVariable(a[0].s);
    if (idx < 0) throw ScriptError{ "save_variable: there is no variable named '" + a[0].s + "'" };
    const VarDef& def = *inst.GetProgram().vars[(size_t)idx];
    std::string file = a.size() > 1 && !a[1].s.empty() ? a[1].s : def.saveFile;
    if (file.empty()) throw ScriptError{ "save_variable: '" + a[0].s + "' has no save file; add @save(\"file\") or pass a file name" };
    if (def.type.kind == Kind::Array || def.type.kind == Kind::Actor || def.type.kind == Kind::Component || def.type.kind == Kind::Hit)
        throw ScriptError{ "save_variable: variables of type " + def.type.ToString() + " cannot be saved this way" };
    SaveFileFormat::WriteEntries(file, { { VariableKey(owner, a[0].s), Serialize(inst.GlobalAt(idx)) } });
    return Value::Void();
}

Value ReadSaved(Runtime& rt, std::vector<Value>& a, bool assign) {
    Instance& inst = RequireInstance(rt);
    Actor* owner = RequireOwner(rt, "load_variable");
    int idx = inst.FindVariable(a[0].s);
    if (idx < 0) throw ScriptError{ "There is no variable named '" + a[0].s + "'" };
    const VarDef& def = *inst.GetProgram().vars[(size_t)idx];
    std::string file = a.size() > 1 && !a[1].s.empty() ? a[1].s : def.saveFile;
    if (file.empty()) return Value::Bool(false);
    auto entries = SaveFileFormat::ReadEntries(file);
    auto it = entries.find(VariableKey(owner, a[0].s));
    if (it == entries.end()) return Value::Bool(false);
    if (assign) inst.GlobalAt(idx) = Deserialize(def.type, it->second);
    return Value::Bool(true);
}
Value Fn_load_variable(Runtime& rt, std::vector<Value>& a) { return ReadSaved(rt, a, true); }
Value Fn_has_saved_variable(Runtime& rt, std::vector<Value>& a) { return ReadSaved(rt, a, false); }

Value Fn_set_save_slot(Runtime&, std::vector<Value>& a) { SaveSlots::SetCurrent(a[0].s); return Value::Void(); }
Value Fn_get_save_slot(Runtime&, std::vector<Value>&) { return Value::String(SavePaths::GetCurrentSlot()); }
Value Fn_save_slot_exists(Runtime&, std::vector<Value>& a) { return Value::Bool(SaveSlots::Exists(a[0].s)); }
Value Fn_delete_save_slot(Runtime&, std::vector<Value>& a) { return Value::Bool(SaveSlots::Delete(a[0].s)); }
Value Fn_copy_save_slot(Runtime&, std::vector<Value>& a) { return Value::Bool(SaveSlots::Copy(a[0].s, a[1].s)); }
Value Fn_save_slot_count(Runtime&, std::vector<Value>&) { return Value::Int((long long)SaveSlots::List().size()); }
Value Fn_save_slot_name(Runtime&, std::vector<Value>& a) {
    std::vector<SlotInfo> slots = SaveSlots::List();
    if (a[0].i < 0 || a[0].i >= (long long)slots.size()) return Value::String("");
    return Value::String(slots[(size_t)a[0].i].name);
}

AudioSource* FindSound(const Value& v) { return AudioCore::Get().FindInstance((int)v.i); }

Value Fn_play_sound(Runtime&, std::vector<Value>& a) {
    float volume = a.size() > 1 ? (float)a[1].AsNumber() : 1.0f;
    float pitch = a.size() > 2 ? (float)a[2].AsNumber() : 1.0f;
    bool looping = a.size() > 3 ? a[3].b : false;
    std::string bus = a.size() > 4 ? a[4].s : std::string();
    return Value::Int(AudioCore::Get().PlayAsset(a[0].s, volume, pitch, looping, bus));
}
Value Fn_stop_sound(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->Stop(); return Value::Void(); }
Value Fn_pause_sound(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->Pause(); return Value::Void(); }
Value Fn_resume_sound(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->Resume(); return Value::Void(); }
Value Fn_is_sound_playing(Runtime&, std::vector<Value>& a) { AudioSource* s = FindSound(a[0]); return Value::Bool(s && s->IsPlaying()); }
Value Fn_set_sound_volume(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->SetVolume((float)a[1].AsNumber()); return Value::Void(); }
Value Fn_set_sound_pitch(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->SetPitch((float)a[1].AsNumber()); return Value::Void(); }
Value Fn_set_sound_tempo(Runtime&, std::vector<Value>& a) { AudioSource* s = FindSound(a[0]); if (s && s->SupportsTempo()) s->SetTempo((float)a[1].AsNumber()); return Value::Void(); }
Value Fn_seek_sound(Runtime&, std::vector<Value>& a) { if (AudioSource* s = FindSound(a[0])) s->Seek(a[1].AsNumber()); return Value::Void(); }
Value Fn_sound_position(Runtime&, std::vector<Value>& a) { AudioSource* s = FindSound(a[0]); return Value::Float(s ? s->GetPosition() : 0.0); }
Value Fn_sound_duration(Runtime&, std::vector<Value>& a) { AudioSource* s = FindSound(a[0]); return Value::Float(s ? s->GetDuration() : 0.0); }
Value Fn_set_master_volume(Runtime&, std::vector<Value>& a) { AudioCore::Get().SetBusVolume(AudioBusRegistry::MasterName(), (float)a[0].AsNumber()); return Value::Void(); }
Value Fn_set_bus_volume(Runtime&, std::vector<Value>& a) {
    if (!AudioCore::Get().SetBusVolume(a[0].s, (float)a[1].AsNumber())) throw ScriptError{ "Unknown audio bus '" + a[0].s + "'" };
    return Value::Void();
}
Value Fn_get_bus_volume(Runtime&, std::vector<Value>& a) {
    float volume = 1.0f;
    if (!AudioCore::Get().GetBusVolume(a[0].s, volume)) throw ScriptError{ "Unknown audio bus '" + a[0].s + "'" };
    return Value::Float(volume);
}
Value Fn_set_bus_muted(Runtime&, std::vector<Value>& a) {
    if (!AudioCore::Get().SetBusMuted(a[0].s, a[1].b)) throw ScriptError{ "Unknown audio bus '" + a[0].s + "'" };
    return Value::Void();
}
Value Fn_is_bus_muted(Runtime&, std::vector<Value>& a) {
    bool muted = false;
    if (!AudioCore::Get().IsBusMuted(a[0].s, muted)) throw ScriptError{ "Unknown audio bus '" + a[0].s + "'" };
    return Value::Bool(muted);
}

}

const std::vector<BuiltinDef>& ExtraBuiltins() {
    static const std::vector<BuiltinDef> table = {
        { "delta_time", {}, 0, T(Kind::Float), RetRule::Fixed, false, Fn_delta_time },
        { "delta_scale", { T(Kind::Float) }, 0, T(Kind::Float), RetRule::Fixed, false, Fn_delta_scale },
        { "fps", {}, 0, T(Kind::Int), RetRule::Fixed, false, Fn_fps },
        { "set_target_fps", { T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_target_fps },
        { "monitor_size", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, Fn_monitor_size },
        { "mouse_position", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, Fn_mouse_position },
        { "mouse_delta", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, Fn_mouse_delta },
        { "key_pressed", {}, 0, T(Kind::String), RetRule::Fixed, false, Fn_key_pressed },
        { "char_pressed", {}, 0, T(Kind::String), RetRule::Fixed, false, Fn_char_pressed },
        { "set_window_title", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_window_title },
        { "toggle_fullscreen", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_toggle_fullscreen },
        { "maximize_window", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_maximize_window },
        { "minimize_window", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_minimize_window },
        { "is_window_minimized", {}, 0, T(Kind::Bool), RetRule::Fixed, false, Fn_is_window_minimized },
        { "is_window_maximized", {}, 0, T(Kind::Bool), RetRule::Fixed, false, Fn_is_window_maximized },
        { "is_window_focused", {}, 0, T(Kind::Bool), RetRule::Fixed, false, Fn_is_window_focused },
        { "set_window_opacity", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_window_opacity },
        { "window_position", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, Fn_window_position },
        { "set_window_position", { T(Kind::Vec3) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_window_position },
        { "window_size", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, Fn_window_size },
        { "set_window_size", { T(Kind::Vec3) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_window_size },
        { "take_screenshot", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_take_screenshot },
        { "open_url", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_open_url },
        { "get_clipboard", {}, 0, T(Kind::String), RetRule::Fixed, false, Fn_get_clipboard },
        { "set_clipboard", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_clipboard },
        { "file_exists", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_file_exists },
        { "directory_exists", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_directory_exists },
        { "load_text_file", { T(Kind::String) }, 1, T(Kind::String), RetRule::Fixed, false, Fn_load_text_file },
        { "save_text_file", { T(Kind::String), T(Kind::String) }, 2, T(Kind::Bool), RetRule::Fixed, false, Fn_save_text_file },
        { "working_directory", {}, 0, T(Kind::String), RetRule::Fixed, false, Fn_working_directory },
        { "change_directory", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_change_directory },
        { "interp_to", { T(Kind::Float), T(Kind::Float), T(Kind::Float), T(Kind::Float), T(Kind::String) }, 4, T(Kind::Float), RetRule::Fixed, false, Fn_interp_to },
        { "vec3_interp_to", { T(Kind::Vec3), T(Kind::Vec3), T(Kind::Float), T(Kind::Float), T(Kind::String) }, 4, T(Kind::Vec3), RetRule::Fixed, false, Fn_vec3_interp_to },
        { "fmod", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_fmod },
        { "truncate", { T(Kind::Float) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_truncate },
        { "random_bool", {}, 0, T(Kind::Bool), RetRule::Fixed, false, Fn_random_bool },
        { "vec3_reflect", { T(Kind::Vec3), T(Kind::Vec3) }, 2, T(Kind::Vec3), RetRule::Fixed, false, Fn_vec3_reflect },
        { "to_vec3", { T(Kind::String) }, 1, T(Kind::Vec3), RetRule::Fixed, false, Fn_to_vec3 },
        { "is_class", { MakeObjectType(Kind::Actor, "Actor"), T(Kind::String) }, 2, T(Kind::Bool), RetRule::Fixed, false, Fn_is_class },
        { "load_level", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_load_level },
        { "has_authority", {}, 0, T(Kind::Bool), RetRule::Fixed, false, Fn_has_authority },
        { "save_all", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_save_all },
        { "load_all", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_load_all },
        { "save_file", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_save_file },
        { "load_file", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_load_file },
        { "save_game", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_save_game },
        { "load_game", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_load_game },
        { "save_variable", { T(Kind::String), T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_save_variable },
        { "load_variable", { T(Kind::String), T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_load_variable },
        { "has_saved_variable", { T(Kind::String), T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_has_saved_variable },
        { "set_save_slot", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_save_slot },
        { "get_save_slot", {}, 0, T(Kind::String), RetRule::Fixed, false, Fn_get_save_slot },
        { "save_slot_exists", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_save_slot_exists },
        { "delete_save_slot", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_delete_save_slot },
        { "copy_save_slot", { T(Kind::String), T(Kind::String) }, 2, T(Kind::Bool), RetRule::Fixed, false, Fn_copy_save_slot },
        { "save_slot_count", {}, 0, T(Kind::Int), RetRule::Fixed, false, Fn_save_slot_count },
        { "save_slot_name", { T(Kind::Int) }, 1, T(Kind::String), RetRule::Fixed, false, Fn_save_slot_name },
        { "play_sound", { T(Kind::String), T(Kind::Float), T(Kind::Float), T(Kind::Bool), T(Kind::String) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_play_sound },
        { "stop_sound", { T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_stop_sound },
        { "pause_sound", { T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_pause_sound },
        { "resume_sound", { T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_resume_sound },
        { "is_sound_playing", { T(Kind::Int) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_is_sound_playing },
        { "set_sound_volume", { T(Kind::Int), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_sound_volume },
        { "set_sound_pitch", { T(Kind::Int), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_sound_pitch },
        { "set_sound_tempo", { T(Kind::Int), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_sound_tempo },
        { "seek_sound", { T(Kind::Int), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_seek_sound },
        { "sound_position", { T(Kind::Int) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_sound_position },
        { "sound_duration", { T(Kind::Int) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_sound_duration },
        { "set_bus_volume", { T(Kind::String), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_bus_volume },
        { "get_bus_volume", { T(Kind::String) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_get_bus_volume },
        { "set_bus_muted", { T(Kind::String), T(Kind::Bool) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_bus_muted },
        { "is_bus_muted", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_is_bus_muted },
        { "set_master_volume", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_master_volume },
    };
    return table;
}

const std::vector<MethodDef>& ExtraMethods() {
    static const std::vector<MethodDef> table = {
        { Kind::Actor, "is_class", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, M_actor_is_class },
        { Kind::Actor, "get_component", { T(Kind::String) }, 1, CompT(), RetRule::Fixed, false, M_actor_get_component },
    };
    return table;
}

const std::vector<PropertyDef>& ExtraProperties() {
    static const std::vector<PropertyDef> table = {
        { Kind::Actor, "class_name", T(Kind::String), P_actor_class_name, nullptr },
        { Kind::Component, "world_position", T(Kind::Vec3), P_comp_world_position, P_comp_set_world_position },
        { Kind::Component, "world_rotation", T(Kind::Vec3), P_comp_world_rotation, P_comp_set_world_rotation },
        { Kind::Component, "world_scale", T(Kind::Vec3), P_comp_world_scale, P_comp_set_world_scale },
    };
    return table;
}

}
