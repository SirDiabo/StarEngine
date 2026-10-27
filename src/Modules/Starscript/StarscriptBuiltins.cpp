#include "StarscriptBuiltins.h"
#include "StarscriptRuntime.h"
#include "StarscriptInput.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/StarMap.h"
#include "Runtime/ActorClassCache.h"
#include "Runtime/PrintMessageLog.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Components/PhysicsComponent.h"
#include "Runtime/Components/AudioComponent.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/ComponentRegistry.h"
#include "Runtime/CollisionTrace.h"
#include "Runtime/CollisionDebug.h"
#include "Core/CollisionLayers.h"
#include "Core/ObjectKindRegistry.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Core/ObjectRefRegistry.h"
#include "Core/RotationMath.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <unordered_map>

namespace Starscript {

Actor* ResolveActor(const Value& v) {
    if (v.kind != Kind::Actor || v.objectId == 0) return nullptr;
    return dynamic_cast<Actor*>(ObjectRefRegistry::Get().Resolve(v.objectId));
}

Component* ResolveComponent(const Value& v) {
    if (v.kind != Kind::Component || v.objectId == 0) return nullptr;
    return dynamic_cast<Component*>(ObjectRefRegistry::Get().Resolve(v.objectId));
}

Value MakeActorValue(Actor* actor) {
    return Value::Object(Kind::Actor, actor ? actor->GetObjectId() : 0);
}

Value MakeComponentValue(Component* component) {
    return Value::Object(Kind::Component, component ? component->GetObjectId() : 0);
}

namespace {

SType T(Kind k) { return MakeType(k); }
SType ActorT() { return MakeObjectType(Kind::Actor, "Actor"); }

Value V3(const Vector3& v) { return Value::Vec3(v.x, v.y, v.z); }
Vector3 ToV3(const Value& v) { return { v.x, v.y, v.z }; }

std::mt19937& Rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

void Require(bool condition, const char* message) {
    if (!condition) throw ScriptError{ message };
}

Actor* RequireActor(const Value& v, const char* what) {
    Actor* a = ResolveActor(v);
    if (!a) throw ScriptError{ std::string(what) + ": the actor is not valid" };
    return a;
}

Component* RequireComponent(const Value& v, const char* what) {
    Component* c = ResolveComponent(v);
    if (!c) throw ScriptError{ std::string(what) + ": the component is not valid" };
    return c;
}

Value PrintWith(PrintMessageType type, std::vector<Value>& args) {
    float duration = args.size() > 1 ? (float)args[1].AsNumber() : 2.0f;
    PrintMessageLog::Get().Add(type, ToDisplayString(args[0]), duration);
    return Value::Void();
}

Value Fn_print(Runtime&, std::vector<Value>& a) { return PrintWith(PrintMessageType::Info, a); }
Value Fn_print_warning(Runtime&, std::vector<Value>& a) { return PrintWith(PrintMessageType::Warning, a); }
Value Fn_print_error(Runtime&, std::vector<Value>& a) { return PrintWith(PrintMessageType::Error, a); }
Value Fn_str(Runtime&, std::vector<Value>& a) { return Value::String(ToDisplayString(a[0])); }

Value Fn_to_int(Runtime&, std::vector<Value>& a) {
    const Value& v = a[0];
    switch (v.kind) {
        case Kind::Int: return v;
        case Kind::Float: return Value::Int((long long)v.f);
        case Kind::Bool: return Value::Int(v.b ? 1 : 0);
        case Kind::String: return Value::Int(strtoll(v.s.c_str(), nullptr, 10));
        default: throw ScriptError{ "to_int: cannot convert " + SType{ v.kind }.ToString() };
    }
}

Value Fn_to_float(Runtime&, std::vector<Value>& a) {
    const Value& v = a[0];
    switch (v.kind) {
        case Kind::Int: return Value::Float((double)v.i);
        case Kind::Float: return v;
        case Kind::Bool: return Value::Float(v.b ? 1.0 : 0.0);
        case Kind::String: return Value::Float(strtod(v.s.c_str(), nullptr));
        default: throw ScriptError{ "to_float: cannot convert " + SType{ v.kind }.ToString() };
    }
}

Value Num(const Value& like, double r) {
    return like.kind == Kind::Int ? Value::Int((long long)r) : Value::Float(r);
}

bool AllInts(const std::vector<Value>& a) {
    for (const Value& v : a) if (v.kind != Kind::Int) return false;
    return true;
}

Value Fn_abs(Runtime&, std::vector<Value>& a) { return a[0].kind == Kind::Int ? Value::Int(std::llabs(a[0].i)) : Value::Float(std::fabs(a[0].f)); }

Value Fn_min(Runtime&, std::vector<Value>& a) {
    double r = std::min(a[0].AsNumber(), a[1].AsNumber());
    return AllInts(a) ? Value::Int((long long)r) : Value::Float(r);
}

Value Fn_max(Runtime&, std::vector<Value>& a) {
    double r = std::max(a[0].AsNumber(), a[1].AsNumber());
    return AllInts(a) ? Value::Int((long long)r) : Value::Float(r);
}

Value Fn_clamp(Runtime&, std::vector<Value>& a) {
    double r = std::min(std::max(a[0].AsNumber(), a[1].AsNumber()), a[2].AsNumber());
    return AllInts(a) ? Value::Int((long long)r) : Value::Float(r);
}

Value Fn_floor(Runtime&, std::vector<Value>& a) { return Value::Int((long long)std::floor(a[0].AsNumber())); }
Value Fn_ceil(Runtime&, std::vector<Value>& a) { return Value::Int((long long)std::ceil(a[0].AsNumber())); }
Value Fn_round(Runtime&, std::vector<Value>& a) { return Value::Int((long long)std::llround(a[0].AsNumber())); }
Value Fn_sqrt(Runtime&, std::vector<Value>& a) {
    Require(a[0].AsNumber() >= 0.0, "sqrt: negative number");
    return Value::Float(std::sqrt(a[0].AsNumber()));
}
Value Fn_pow(Runtime&, std::vector<Value>& a) { return Value::Float(std::pow(a[0].AsNumber(), a[1].AsNumber())); }
Value Fn_sin(Runtime&, std::vector<Value>& a) { return Value::Float(std::sin(a[0].AsNumber())); }
Value Fn_cos(Runtime&, std::vector<Value>& a) { return Value::Float(std::cos(a[0].AsNumber())); }
Value Fn_tan(Runtime&, std::vector<Value>& a) { return Value::Float(std::tan(a[0].AsNumber())); }
Value Fn_atan2(Runtime&, std::vector<Value>& a) { return Value::Float(std::atan2(a[0].AsNumber(), a[1].AsNumber())); }
Value Fn_radians(Runtime&, std::vector<Value>& a) { return Value::Float(a[0].AsNumber() * (PI / 180.0)); }
Value Fn_degrees(Runtime&, std::vector<Value>& a) { return Value::Float(a[0].AsNumber() * (180.0 / PI)); }
Value Fn_lerp(Runtime&, std::vector<Value>& a) { return Value::Float(a[0].AsNumber() + (a[1].AsNumber() - a[0].AsNumber()) * a[2].AsNumber()); }

Value Fn_random(Runtime&, std::vector<Value>&) {
    return Value::Float(std::uniform_real_distribution<double>(0.0, 1.0)(Rng()));
}
Value Fn_random_range(Runtime&, std::vector<Value>& a) {
    double lo = std::min(a[0].AsNumber(), a[1].AsNumber()), hi = std::max(a[0].AsNumber(), a[1].AsNumber());
    return Value::Float(std::uniform_real_distribution<double>(lo, hi)(Rng()));
}
Value Fn_random_int(Runtime&, std::vector<Value>& a) {
    long long lo = std::min(a[0].i, a[1].i), hi = std::max(a[0].i, a[1].i);
    return Value::Int(std::uniform_int_distribution<long long>(lo, hi)(Rng()));
}
Value Fn_game_time(Runtime&, std::vector<Value>&) { return Value::Float(GetTime()); }

Value Fn_len(Runtime&, std::vector<Value>& a) {
    if (a[0].kind == Kind::String) return Value::Int((long long)a[0].s.size());
    if (a[0].kind == Kind::Array) return Value::Int(a[0].arr ? (long long)a[0].arr->size() : 0);
    throw ScriptError{ "len: expected a string or an array" };
}

Value Fn_dot(Runtime&, std::vector<Value>& a) { return Value::Float(a[0].x * a[1].x + a[0].y * a[1].y + a[0].z * a[1].z); }
Value Fn_cross(Runtime&, std::vector<Value>& a) {
    return Value::Vec3(a[0].y * a[1].z - a[0].z * a[1].y, a[0].z * a[1].x - a[0].x * a[1].z, a[0].x * a[1].y - a[0].y * a[1].x);
}
Value Fn_distance(Runtime&, std::vector<Value>& a) {
    float dx = a[0].x - a[1].x, dy = a[0].y - a[1].y, dz = a[0].z - a[1].z;
    return Value::Float(std::sqrt(dx * dx + dy * dy + dz * dz));
}

Value Fn_destroy_actor(Runtime& rt, std::vector<Value>& a) {
    Actor* target = RequireActor(a[0], "destroy_actor");
    if (ActorBehavior* behavior = target->GetBehavior()) behavior->OnDestroyed(target);
    World::Get().DestroyActor(target);
    return Value::Void();
}

Value Fn_spawn_actor(Runtime& rt, std::vector<Value>& a) {
    const std::string& classPath = a[0].s;
    if (classPath.empty() || !ActorClassCache::Get().GetOrLoad(classPath)) throw ScriptError{ "spawn_actor: could not load class '" + classPath + "'" };

    ActorInstance instance;
    instance.className = "Actor";
    instance.scaleX = instance.scaleY = instance.scaleZ = 1.0f;
    if (a.size() > 1) { instance.posX = a[1].x; instance.posY = a[1].y; instance.posZ = a[1].z; }
    if (a.size() > 2) { instance.rotX = a[2].x; instance.rotY = a[2].y; instance.rotZ = a[2].z; }

    size_t slash = classPath.find_last_of("/\\");
    std::string filename = slash == std::string::npos ? classPath : classPath.substr(slash + 1);
    size_t dot = filename.find_last_of('.');
    std::string stem = dot == std::string::npos ? filename : filename.substr(0, dot);
    instance.name = (stem.empty() ? std::string("Actor") : stem) + "_Instance";
    instance.classAssetPath = classPath;

    Actor* spawned = WorldSpawner::SpawnActorFromInstance(instance);
    if (!spawned) throw ScriptError{ "spawn_actor: spawning failed" };

    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (backend && backend->IsHost()) {
        spawned->SetNetId(backend->AllocateNetId());
        ReplicatedActorSpawn spawn;
        spawn.netId = spawned->GetNetId();
        spawn.className = instance.className;
        spawn.graphPath = classPath;
        spawn.posX = spawned->position.x; spawn.posY = spawned->position.y; spawn.posZ = spawned->position.z;
        spawn.rotX = spawned->rotation.x; spawn.rotY = spawned->rotation.y; spawn.rotZ = spawned->rotation.z;
        spawn.scaleX = spawned->scale.x; spawn.scaleY = spawned->scale.y; spawn.scaleZ = spawned->scale.z;
        backend->SendActorSpawn(spawn);
    }
    return MakeActorValue(spawned);
}

std::string SerializeForEvent(const Value& v) {
    if (v.kind == Kind::Actor || v.kind == Kind::Component) {
        if (v.objectId == 0) return std::string();
        if (Object* obj = ObjectRefRegistry::Get().Resolve(v.objectId))
            return ObjectRefRegistry::Encode(obj->GetObjectKind(), v.objectId);
        return std::string();
    }
    return Serialize(v);
}

Value Fn_send_event(Runtime& rt, std::vector<Value>& a) {
    Actor* target = RequireActor(a[0], "send_event");
    const std::string& eventName = a[1].s;
    EventParams params;
    for (size_t n = 2; n < a.size(); n++) params.emplace_back("p" + std::to_string(n - 2), SerializeForEvent(a[n]));

    if (ActorBehavior* behavior = target->GetBehavior()) behavior->OnCustomEvent(target, eventName, params);

    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (backend && target->GetNetId() != 0) {
        ActorBehavior* behavior = target->GetBehavior();
        ReplicationMode mode = ReplicationMode::Local;
        if (behavior && behavior->FindEventReplication(eventName, mode) && mode != ReplicationMode::Local) {
            bool allowed = mode == ReplicationMode::Replicated || backend->HasAuthority();
            if (allowed) {
                ReplicatedEventCall call;
                call.netId = target->GetNetId();
                call.eventName = eventName;
                call.params = params;
                backend->SendEventCall(call, false);
            }
        }
    }
    return Value::Void();
}


Value Fn_set_view_target(Runtime&, std::vector<Value>& a) { World::Get().SetViewTarget(ResolveActor(a[0])); return Value::Void(); }
Value Fn_get_view_target(Runtime&, std::vector<Value>&) { return MakeActorValue(World::Get().GetViewTarget()); }
Value Fn_set_input_focus(Runtime&, std::vector<Value>& a) { World::Get().SetInputFocus(ResolveActor(a[0])); return Value::Void(); }
Value Fn_get_input_focus(Runtime&, std::vector<Value>&) { return MakeActorValue(World::Get().GetInputFocus()); }
Value Fn_quit_game(Runtime&, std::vector<Value>&) { World::Get().RequestQuit(); return Value::Void(); }

Value Fn_get_actor_of_class(Runtime&, std::vector<Value>& a) {
    for (const auto& actorPtr : World::Get().GetActors()) {
        if (ObjectKindRegistry::Get().IsCompatible(actorPtr->GetObjectKind(), a[0].s)) return MakeActorValue(actorPtr.get());
    }
    return Value::Object(Kind::Actor, 0);
}

Value Fn_get_actors_of_class(Runtime&, std::vector<Value>& a) {
    Value result = Value::Array();
    for (const auto& actorPtr : World::Get().GetActors()) {
        if (ObjectKindRegistry::Get().IsCompatible(actorPtr->GetObjectKind(), a[0].s)) result.arr->push_back(MakeActorValue(actorPtr.get()));
    }
    return result;
}

Value Fn_get_all_actors(Runtime&, std::vector<Value>&) {
    Value result = Value::Array();
    for (const auto& actorPtr : World::Get().GetActors()) result.arr->push_back(MakeActorValue(actorPtr.get()));
    return result;
}

Value Fn_line_trace(Runtime& rt, std::vector<Value>& a) {
    Vector3 start = ToV3(a[0]), end = ToV3(a[1]);
    bool ignoreSelf = a.size() > 2 ? a[2].b : true;
    std::string channel = a.size() > 3 ? a[3].s : std::string();
    bool includeTriggers = a.size() > 4 ? a[4].b : false;
    bool drawDebug = a.size() > 5 ? a[5].b : false;

    CollisionTrace::QueryParams params;
    params.ignoreActor = ignoreSelf ? rt.owner : nullptr;
    params.shapes = true;
    params.triggers = includeTriggers ? CollisionTrace::TriggerMode::Include : CollisionTrace::TriggerMode::Ignore;
    params.layer = channel.empty() || channel == "All" ? -1 : CollisionLayers::IndexOf(channel);

    CollisionTrace::HitResult hit;
    bool didHit = CollisionTrace::Raycast(start, end, params, hit);

    Value result = Value::Default(MakeType(Kind::Hit));
    (*result.arr)[0] = Value::Bool(didHit);
    (*result.arr)[2] = V3(didHit ? hit.point : end);
    (*result.arr)[4] = Value::Float(didHit ? hit.distance : Vector3Distance(start, end));
    if (didHit) {
        (*result.arr)[1] = MakeActorValue(hit.actor);
        (*result.arr)[3] = V3(hit.normal);
        (*result.arr)[5] = Value::Bool(hit.isTrigger);
    }
    if (drawDebug && IsCollisionDebugEnabled()) {
        AddCollisionDebugLine(start, didHit ? hit.point : end, didHit ? RED : GREEN);
        if (didHit) AddCollisionDebugSphere(hit.point, 3.0f, RED);
    }
    return result;
}

Value M_actor_add_movement_input(Runtime&, Value& s, std::vector<Value>& a) {
    RequireActor(s, "add_movement_input")->AddMovementInput(ToV3(a[0]), a.size() > 1 ? (float)a[1].AsNumber() : 1.0f);
    return Value::Void();
}
Value M_actor_add_yaw(Runtime&, Value& s, std::vector<Value>& a) { RequireActor(s, "add_control_yaw_input")->AddControlYawInput((float)a[0].AsNumber()); return Value::Void(); }
Value M_actor_add_pitch(Runtime&, Value& s, std::vector<Value>& a) { RequireActor(s, "add_control_pitch_input")->AddControlPitchInput((float)a[0].AsNumber()); return Value::Void(); }
Value M_actor_add_roll(Runtime&, Value& s, std::vector<Value>& a) { RequireActor(s, "add_control_roll_input")->AddControlRollInput((float)a[0].AsNumber()); return Value::Void(); }

PhysicsComponent* RequirePhysics(const Value& v, const char* what) {
    PhysicsComponent* p = dynamic_cast<PhysicsComponent*>(RequireComponent(v, what));
    if (!p) throw ScriptError{ std::string(what) + ": the component is not a PhysicsComponent" };
    return p;
}
AudioComponent* RequireAudio(const Value& v, const char* what) {
    AudioComponent* c = dynamic_cast<AudioComponent*>(RequireComponent(v, what));
    if (!c) throw ScriptError{ std::string(what) + ": the component is not an AudioComponent" };
    return c;
}
MeshComponent* RequireMesh(const Value& v, const char* what) {
    MeshComponent* c = dynamic_cast<MeshComponent*>(RequireComponent(v, what));
    if (!c) throw ScriptError{ std::string(what) + ": the component is not a MeshComponent" };
    return c;
}

Value M_phys_add_impulse(Runtime&, Value& s, std::vector<Value>& a) { RequirePhysics(s, "add_impulse")->AddImpulse(ToV3(a[0])); return Value::Void(); }
Value M_audio_play(Runtime&, Value& s, std::vector<Value>&) { RequireAudio(s, "play")->Play(); return Value::Void(); }
Value M_audio_stop(Runtime&, Value& s, std::vector<Value>&) { RequireAudio(s, "stop")->Stop(); return Value::Void(); }
Value M_audio_pause(Runtime&, Value& s, std::vector<Value>&) { RequireAudio(s, "pause")->Pause(); return Value::Void(); }
Value M_audio_resume(Runtime&, Value& s, std::vector<Value>&) { RequireAudio(s, "resume")->Resume(); return Value::Void(); }
Value M_mesh_play_animation(Runtime&, Value& s, std::vector<Value>& a) {
    RequireMesh(s, "play_animation")->PlayAnimation(a[0].s, a.size() > 1 ? a[1].b : false);
    return Value::Void();
}
Value M_mesh_stop_animation(Runtime&, Value& s, std::vector<Value>&) { RequireMesh(s, "stop_animation")->StopAnimation(); return Value::Void(); }
Value M_mesh_set_anim_rate(Runtime&, Value& s, std::vector<Value>& a) { RequireMesh(s, "set_anim_rate")->SetAnimPlayRate((float)a[0].AsNumber()); return Value::Void(); }

Instance& RequireInstance(Runtime& rt) {
    if (!rt.instance) throw ScriptError{ "This function needs a running script" };
    return *rt.instance;
}
Value Fn_async_misuse(Runtime&, std::vector<Value>&) { throw ScriptError{ "delay() and next_frame() can only be used with 'await'" }; }
Value Fn_set_timer(Runtime& rt, std::vector<Value>& a) {
    float interval = (float)a[1].AsNumber();
    if (interval < 0.0f) throw ScriptError{ "set_timer: the time cannot be negative" };
    RequireInstance(rt).SetTimer(a[0].s, interval, a.size() > 2 ? a[2].b : false);
    return Value::Void();
}
Value Fn_clear_timer(Runtime& rt, std::vector<Value>& a) { RequireInstance(rt).ClearTimer(a[0].s); return Value::Void(); }
Value Fn_pause_timer(Runtime& rt, std::vector<Value>& a) { RequireInstance(rt).SetTimerPaused(a[0].s, true); return Value::Void(); }
Value Fn_resume_timer(Runtime& rt, std::vector<Value>& a) { RequireInstance(rt).SetTimerPaused(a[0].s, false); return Value::Void(); }
Value Fn_is_timer_active(Runtime& rt, std::vector<Value>& a) { return Value::Bool(RequireInstance(rt).IsTimerActive(a[0].s)); }
Value Fn_timer_remaining(Runtime& rt, std::vector<Value>& a) { return Value::Float(RequireInstance(rt).TimerRemaining(a[0].s)); }

Value Fn_input_pressed(Runtime& rt, std::vector<Value>& a) { return Value::Bool(RequireInstance(rt).QueryInput(a[0].s).held); }
Value Fn_input_just_pressed(Runtime& rt, std::vector<Value>& a) { const auto& s = RequireInstance(rt).QueryInput(a[0].s); return Value::Bool(s.held && !s.prevHeld); }
Value Fn_input_just_released(Runtime& rt, std::vector<Value>& a) { const auto& s = RequireInstance(rt).QueryInput(a[0].s); return Value::Bool(!s.held && s.prevHeld); }
Value Fn_input_axis(Runtime& rt, std::vector<Value>& a) { return Value::Float(RequireInstance(rt).QueryInput(a[0].s).axis); }
Value Fn_input_axis_2d(Runtime& rt, std::vector<Value>& a) { const auto& s = RequireInstance(rt).QueryInput(a[0].s); return Value::Vec3(s.x, s.y, 0.0f); }

Value M_vec_length(Runtime&, Value& s, std::vector<Value>&) { return Value::Float(std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z)); }
Value M_vec_normalized(Runtime&, Value& s, std::vector<Value>&) {
    float len = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
    if (len < 1e-8f) return Value::Vec3(0, 0, 0);
    return Value::Vec3(s.x / len, s.y / len, s.z / len);
}

Value M_object_is_valid(Runtime&, Value& s, std::vector<Value>&) {
    return Value::Bool(s.kind == Kind::Actor ? ResolveActor(s) != nullptr : ResolveComponent(s) != nullptr);
}
Value M_actor_destroy(Runtime&, Value& s, std::vector<Value>&) {
    Actor* target = RequireActor(s, "destroy");
    if (ActorBehavior* behavior = target->GetBehavior()) behavior->OnDestroyed(target);
    World::Get().DestroyActor(target);
    return Value::Void();
}

Value M_str_length(Runtime&, Value& s, std::vector<Value>&) { return Value::Int((long long)s.s.size()); }
Value M_str_upper(Runtime&, Value& s, std::vector<Value>&) {
    std::string r = s.s;
    for (char& c : r) c = (char)toupper((unsigned char)c);
    return Value::String(r);
}
Value M_str_lower(Runtime&, Value& s, std::vector<Value>&) {
    std::string r = s.s;
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return Value::String(r);
}
Value M_str_contains(Runtime&, Value& s, std::vector<Value>& a) { return Value::Bool(s.s.find(a[0].s) != std::string::npos); }
Value M_str_find(Runtime&, Value& s, std::vector<Value>& a) {
    size_t p = s.s.find(a[0].s);
    return Value::Int(p == std::string::npos ? -1 : (long long)p);
}
Value M_str_substr(Runtime&, Value& s, std::vector<Value>& a) {
    long long start = a[0].i;
    if (start < 0 || start > (long long)s.s.size()) throw ScriptError{ "substr: start is out of range" };
    if (a.size() > 1) {
        long long len = a[1].i;
        if (len < 0) throw ScriptError{ "substr: length cannot be negative" };
        return Value::String(s.s.substr((size_t)start, (size_t)len));
    }
    return Value::String(s.s.substr((size_t)start));
}
Value M_str_replace(Runtime&, Value& s, std::vector<Value>& a) {
    if (a[0].s.empty()) return s;
    std::string r = s.s;
    size_t pos = 0;
    while ((pos = r.find(a[0].s, pos)) != std::string::npos) {
        r.replace(pos, a[0].s.size(), a[1].s);
        pos += a[1].s.size();
    }
    return Value::String(r);
}
Value M_str_starts_with(Runtime&, Value& s, std::vector<Value>& a) { return Value::Bool(s.s.rfind(a[0].s, 0) == 0); }
Value M_str_ends_with(Runtime&, Value& s, std::vector<Value>& a) {
    return Value::Bool(s.s.size() >= a[0].s.size() && s.s.compare(s.s.size() - a[0].s.size(), a[0].s.size(), a[0].s) == 0);
}



Value M_arr_size(Runtime&, Value& s, std::vector<Value>&) { return Value::Int(s.arr ? (long long)s.arr->size() : 0); }
Value M_arr_append(Runtime&, Value& s, std::vector<Value>& a) {
    if (!s.arr) s.arr = std::make_shared<std::vector<Value>>();
    s.arr->push_back(DeepCopy(a[0]));
    return Value::Void();
}
Value M_arr_clear(Runtime&, Value& s, std::vector<Value>&) { if (s.arr) s.arr->clear(); return Value::Void(); }
Value M_arr_pop(Runtime&, Value& s, std::vector<Value>&) {
    if (!s.arr || s.arr->empty()) throw ScriptError{ "pop: the array is empty" };
    Value v = s.arr->back();
    s.arr->pop_back();
    return v;
}
Value M_arr_remove_at(Runtime&, Value& s, std::vector<Value>& a) {
    if (!s.arr || a[0].i < 0 || a[0].i >= (long long)s.arr->size()) throw ScriptError{ "remove_at: index is out of range" };
    s.arr->erase(s.arr->begin() + (long)a[0].i);
    return Value::Void();
}
Value M_arr_contains(Runtime&, Value& s, std::vector<Value>& a) {
    if (!s.arr) return Value::Bool(false);
    for (const Value& e : *s.arr) {
        bool same = false;
        if (e.kind == Kind::Int || e.kind == Kind::Float) same = (a[0].kind == Kind::Int || a[0].kind == Kind::Float) && e.AsNumber() == a[0].AsNumber();
        else if (e.kind == Kind::Bool) same = e.b == a[0].b;
        else if (e.kind == Kind::String) same = e.s == a[0].s;
        else if (e.kind == Kind::Vec3) same = e.x == a[0].x && e.y == a[0].y && e.z == a[0].z;
        else if (e.kind == Kind::Actor || e.kind == Kind::Component) same = e.objectId == a[0].objectId;
        if (same) return Value::Bool(true);
    }
    return Value::Bool(false);
}

Value P_actor_get_position(Runtime&, const Value& s) { return V3(RequireActor(s, "position")->position); }
void P_actor_set_position(Runtime&, const Value& s, const Value& v) { RequireActor(s, "position")->position = ToV3(v); }
Value P_actor_get_rotation(Runtime&, const Value& s) { return V3(RequireActor(s, "rotation")->rotation); }
void P_actor_set_rotation(Runtime&, const Value& s, const Value& v) { RequireActor(s, "rotation")->rotation = ToV3(v); }
Value P_actor_get_scale(Runtime&, const Value& s) { return V3(RequireActor(s, "scale")->scale); }
void P_actor_set_scale(Runtime&, const Value& s, const Value& v) { RequireActor(s, "scale")->scale = ToV3(v); }
Value P_actor_get_name(Runtime&, const Value& s) { return Value::String(RequireActor(s, "name")->GetName()); }
Value P_actor_get_forward(Runtime&, const Value& s) {
    Matrix m = ComposeRotationDegrees(RequireActor(s, "forward")->rotation);
    return Value::Vec3(m.m8, m.m9, m.m10);
}
Value P_actor_get_right(Runtime&, const Value& s) {
    Matrix m = ComposeRotationDegrees(RequireActor(s, "right")->rotation);
    return Value::Vec3(-m.m0, -m.m1, -m.m2);
}
Value P_actor_get_up(Runtime&, const Value& s) {
    Matrix m = ComposeRotationDegrees(RequireActor(s, "up")->rotation);
    return Value::Vec3(m.m4, m.m5, m.m6);
}

Value P_actor_get_show_cursor(Runtime&, const Value& s) { return Value::Bool(RequireActor(s, "show_mouse_cursor")->showMouseCursor); }
void P_actor_set_show_cursor(Runtime&, const Value& s, const Value& v) { RequireActor(s, "show_mouse_cursor")->showMouseCursor = v.b; }
Value P_actor_get_control_rotation(Runtime&, const Value& s) { return V3(RequireActor(s, "control_rotation")->controlRotation); }
void P_actor_set_control_rotation(Runtime&, const Value& s, const Value& v) { RequireActor(s, "control_rotation")->controlRotation = ToV3(v); }
Value P_actor_get_movement_speed(Runtime&, const Value& s) { return Value::Float(RequireActor(s, "movement_speed")->MovementSpeed); }
void P_actor_set_movement_speed(Runtime&, const Value& s, const Value& v) { RequireActor(s, "movement_speed")->MovementSpeed = (float)v.AsNumber(); }

Value P_phys_get_velocity(Runtime&, const Value& s) { return V3(RequirePhysics(s, "velocity")->velocity); }
void P_phys_set_velocity(Runtime&, const Value& s, const Value& v) { RequirePhysics(s, "velocity")->SetVelocity(ToV3(v)); }
Value P_phys_get_grounded(Runtime&, const Value& s) { return Value::Bool(RequirePhysics(s, "grounded")->grounded); }
Value P_phys_get_ground_normal(Runtime&, const Value& s) { return V3(RequirePhysics(s, "ground_normal")->groundNormal); }

Value HitField(const Value& s, size_t index) {
    if (!s.arr || index >= s.arr->size()) throw ScriptError{ "Invalid Hit value" };
    return (*s.arr)[index];
}
Value P_hit_did_hit(Runtime&, const Value& s) { return HitField(s, 0); }
Value P_hit_actor(Runtime&, const Value& s) { return HitField(s, 1); }
Value P_hit_location(Runtime&, const Value& s) { return HitField(s, 2); }
Value P_hit_normal(Runtime&, const Value& s) { return HitField(s, 3); }
Value P_hit_distance(Runtime&, const Value& s) { return HitField(s, 4); }
Value P_hit_is_trigger(Runtime&, const Value& s) { return HitField(s, 5); }

Value P_comp_get_position(Runtime&, const Value& s) { return V3(RequireComponent(s, "position")->position); }
void P_comp_set_position(Runtime&, const Value& s, const Value& v) { RequireComponent(s, "position")->position = ToV3(v); }
Value P_comp_get_rotation(Runtime&, const Value& s) { return V3(RequireComponent(s, "rotation")->rotation); }
void P_comp_set_rotation(Runtime&, const Value& s, const Value& v) { RequireComponent(s, "rotation")->rotation = ToV3(v); }
Value P_comp_get_scale(Runtime&, const Value& s) { return V3(RequireComponent(s, "scale")->scale); }
void P_comp_set_scale(Runtime&, const Value& s, const Value& v) { RequireComponent(s, "scale")->scale = ToV3(v); }
Value P_comp_get_name(Runtime&, const Value& s) { return Value::String(RequireComponent(s, "name")->GetName()); }

const std::vector<BuiltinDef>& Builtins() {
    static const std::vector<BuiltinDef> table = {
        { "print", { T(Kind::Any), T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_print },
        { "print_warning", { T(Kind::Any), T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_print_warning },
        { "print_error", { T(Kind::Any), T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_print_error },
        { "str", { T(Kind::Any) }, 1, T(Kind::String), RetRule::Fixed, false, Fn_str },
        { "to_int", { T(Kind::Any) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_to_int },
        { "to_float", { T(Kind::Any) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_to_float },
        { "len", { T(Kind::Any) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_len },
        { "abs", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Numeric, false, Fn_abs },
        { "min", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Numeric, false, Fn_min },
        { "max", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Numeric, false, Fn_max },
        { "clamp", { T(Kind::Float), T(Kind::Float), T(Kind::Float) }, 3, T(Kind::Float), RetRule::Numeric, false, Fn_clamp },
        { "floor", { T(Kind::Float) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_floor },
        { "ceil", { T(Kind::Float) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_ceil },
        { "round", { T(Kind::Float) }, 1, T(Kind::Int), RetRule::Fixed, false, Fn_round },
        { "sqrt", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_sqrt },
        { "pow", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_pow },
        { "sin", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_sin },
        { "cos", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_cos },
        { "tan", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_tan },
        { "atan2", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_atan2 },
        { "radians", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_radians },
        { "degrees", { T(Kind::Float) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_degrees },
        { "lerp", { T(Kind::Float), T(Kind::Float), T(Kind::Float) }, 3, T(Kind::Float), RetRule::Fixed, false, Fn_lerp },
        { "random", {}, 0, T(Kind::Float), RetRule::Fixed, false, Fn_random },
        { "random_range", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_random_range },
        { "random_int", { T(Kind::Int), T(Kind::Int) }, 2, T(Kind::Int), RetRule::Fixed, false, Fn_random_int },
        { "game_time", {}, 0, T(Kind::Float), RetRule::Fixed, false, Fn_game_time },
        { "dot", { T(Kind::Vec3), T(Kind::Vec3) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_dot },
        { "cross", { T(Kind::Vec3), T(Kind::Vec3) }, 2, T(Kind::Vec3), RetRule::Fixed, false, Fn_cross },
        { "distance", { T(Kind::Vec3), T(Kind::Vec3) }, 2, T(Kind::Float), RetRule::Fixed, false, Fn_distance },
        { "spawn_actor", { T(Kind::String), T(Kind::Vec3), T(Kind::Vec3) }, 1, ActorT(), RetRule::Fixed, false, Fn_spawn_actor },
        { "destroy_actor", { ActorT() }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_destroy_actor },
        { "send_event", { ActorT(), T(Kind::String), T(Kind::Any) }, 2, T(Kind::Void), RetRule::Fixed, true, Fn_send_event },
        { "delay", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_async_misuse, 1 },
        { "next_frame", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_async_misuse, 2 },
        { "set_timer", { T(Kind::String), T(Kind::Float), T(Kind::Bool) }, 2, T(Kind::Void), RetRule::Fixed, false, Fn_set_timer },
        { "clear_timer", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_clear_timer },
        { "pause_timer", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_pause_timer },
        { "resume_timer", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_resume_timer },
        { "is_timer_active", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_is_timer_active },
        { "timer_remaining", { T(Kind::String) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_timer_remaining },
        { "input_pressed", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_input_pressed },
        { "input_just_pressed", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_input_just_pressed },
        { "input_just_released", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, Fn_input_just_released },
        { "input_axis", { T(Kind::String) }, 1, T(Kind::Float), RetRule::Fixed, false, Fn_input_axis },
        { "input_axis_2d", { T(Kind::String) }, 1, T(Kind::Vec3), RetRule::Fixed, false, Fn_input_axis_2d },
        { "set_view_target", { ActorT() }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_view_target },
        { "get_view_target", {}, 0, ActorT(), RetRule::Fixed, false, Fn_get_view_target },
        { "set_input_focus", { ActorT() }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_input_focus },
        { "set_input_target", { ActorT() }, 1, T(Kind::Void), RetRule::Fixed, false, Fn_set_input_focus },
        { "get_input_focus", {}, 0, ActorT(), RetRule::Fixed, false, Fn_get_input_focus },
        { "get_input_target", {}, 0, ActorT(), RetRule::Fixed, false, Fn_get_input_focus },
        { "get_actor_of_class", { T(Kind::String) }, 1, ActorT(), RetRule::Fixed, false, Fn_get_actor_of_class },
        { "get_actors_of_class", { T(Kind::String) }, 1, MakeArrayType(Kind::Actor), RetRule::Fixed, false, Fn_get_actors_of_class },
        { "get_all_actors", {}, 0, MakeArrayType(Kind::Actor), RetRule::Fixed, false, Fn_get_all_actors },
        { "quit_game", {}, 0, T(Kind::Void), RetRule::Fixed, false, Fn_quit_game },
        { "line_trace", { T(Kind::Vec3), T(Kind::Vec3), T(Kind::Bool), T(Kind::String), T(Kind::Bool), T(Kind::Bool) }, 2, T(Kind::Hit), RetRule::Fixed, false, Fn_line_trace },
    };
    return table;
}

const std::vector<MethodDef>& Methods() {
    static const std::vector<MethodDef> table = {
        { Kind::Vec3, "length", {}, 0, T(Kind::Float), RetRule::Fixed, false, M_vec_length },
        { Kind::Vec3, "normalized", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, M_vec_normalized },
        { Kind::Actor, "add_movement_input", { T(Kind::Vec3), T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_actor_add_movement_input },
        { Kind::Actor, "add_control_yaw_input", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_actor_add_yaw },
        { Kind::Actor, "add_control_pitch_input", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_actor_add_pitch },
        { Kind::Actor, "add_control_roll_input", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_actor_add_roll },
        { Kind::Component, "add_impulse", { T(Kind::Vec3) }, 1, T(Kind::Void), RetRule::Fixed, false, M_phys_add_impulse, "PhysicsComponent" },
        { Kind::Component, "play", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_audio_play, "AudioComponent" },
        { Kind::Component, "stop", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_audio_stop, "AudioComponent" },
        { Kind::Component, "pause", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_audio_pause, "AudioComponent" },
        { Kind::Component, "resume", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_audio_resume, "AudioComponent" },
        { Kind::Component, "play_animation", { T(Kind::String), T(Kind::Bool) }, 1, T(Kind::Void), RetRule::Fixed, false, M_mesh_play_animation, "MeshComponent" },
        { Kind::Component, "stop_animation", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_mesh_stop_animation, "MeshComponent" },
        { Kind::Component, "set_anim_rate", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_mesh_set_anim_rate, "MeshComponent" },
        { Kind::Actor, "is_valid", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_object_is_valid },
        { Kind::Actor, "destroy", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_actor_destroy },
        { Kind::Component, "is_valid", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_object_is_valid },
        { Kind::String, "length", {}, 0, T(Kind::Int), RetRule::Fixed, false, M_str_length },
        { Kind::String, "upper", {}, 0, T(Kind::String), RetRule::Fixed, false, M_str_upper },
        { Kind::String, "lower", {}, 0, T(Kind::String), RetRule::Fixed, false, M_str_lower },
        { Kind::String, "contains", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, M_str_contains },
        { Kind::String, "find", { T(Kind::String) }, 1, T(Kind::Int), RetRule::Fixed, false, M_str_find },
        { Kind::String, "substr", { T(Kind::Int), T(Kind::Int) }, 1, T(Kind::String), RetRule::Fixed, false, M_str_substr },
        { Kind::String, "replace", { T(Kind::String), T(Kind::String) }, 2, T(Kind::String), RetRule::Fixed, false, M_str_replace },
        { Kind::String, "starts_with", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, M_str_starts_with },
        { Kind::String, "ends_with", { T(Kind::String) }, 1, T(Kind::Bool), RetRule::Fixed, false, M_str_ends_with },
        { Kind::Array, "size", {}, 0, T(Kind::Int), RetRule::Fixed, false, M_arr_size },
        { Kind::Array, "append", { T(Kind::Any) }, 1, T(Kind::Void), RetRule::Fixed, true, M_arr_append },
        { Kind::Array, "clear", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_arr_clear },
        { Kind::Array, "pop", {}, 0, T(Kind::Void), RetRule::ElemType, false, M_arr_pop },
        { Kind::Array, "remove_at", { T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, M_arr_remove_at },
        { Kind::Array, "contains", { T(Kind::Any) }, 1, T(Kind::Bool), RetRule::Fixed, true, M_arr_contains },
    };
    return table;
}

const std::vector<PropertyDef>& Properties() {
    static const std::vector<PropertyDef> table = {
        { Kind::Actor, "position", T(Kind::Vec3), P_actor_get_position, P_actor_set_position },
        { Kind::Actor, "rotation", T(Kind::Vec3), P_actor_get_rotation, P_actor_set_rotation },
        { Kind::Actor, "scale", T(Kind::Vec3), P_actor_get_scale, P_actor_set_scale },
        { Kind::Actor, "name", T(Kind::String), P_actor_get_name, nullptr },
        { Kind::Actor, "forward", T(Kind::Vec3), P_actor_get_forward, nullptr },
        { Kind::Actor, "right", T(Kind::Vec3), P_actor_get_right, nullptr },
        { Kind::Actor, "up", T(Kind::Vec3), P_actor_get_up, nullptr },
        { Kind::Actor, "show_mouse_cursor", T(Kind::Bool), P_actor_get_show_cursor, P_actor_set_show_cursor },
        { Kind::Actor, "control_rotation", T(Kind::Vec3), P_actor_get_control_rotation, P_actor_set_control_rotation },
        { Kind::Actor, "movement_speed", T(Kind::Float), P_actor_get_movement_speed, P_actor_set_movement_speed },
        { Kind::Component, "velocity", T(Kind::Vec3), P_phys_get_velocity, P_phys_set_velocity, "PhysicsComponent" },
        { Kind::Component, "grounded", T(Kind::Bool), P_phys_get_grounded, nullptr, "PhysicsComponent" },
        { Kind::Component, "ground_normal", T(Kind::Vec3), P_phys_get_ground_normal, nullptr, "PhysicsComponent" },
        { Kind::Hit, "did_hit", T(Kind::Bool), P_hit_did_hit, nullptr },
        { Kind::Hit, "actor", ActorT(), P_hit_actor, nullptr },
        { Kind::Hit, "location", T(Kind::Vec3), P_hit_location, nullptr },
        { Kind::Hit, "normal", T(Kind::Vec3), P_hit_normal, nullptr },
        { Kind::Hit, "distance", T(Kind::Float), P_hit_distance, nullptr },
        { Kind::Hit, "is_trigger", T(Kind::Bool), P_hit_is_trigger, nullptr },
        { Kind::Component, "position", T(Kind::Vec3), P_comp_get_position, P_comp_set_position },
        { Kind::Component, "rotation", T(Kind::Vec3), P_comp_get_rotation, P_comp_set_rotation },
        { Kind::Component, "scale", T(Kind::Vec3), P_comp_get_scale, P_comp_set_scale },
        { Kind::Component, "name", T(Kind::String), P_comp_get_name, nullptr },
    };
    return table;
}

}

const std::vector<BuiltinDef>& AllBuiltins() { return Builtins(); }
const std::vector<MethodDef>& AllMethods() { return Methods(); }
const std::vector<PropertyDef>& AllProperties() { return Properties(); }

const BuiltinDef* FindBuiltin(const std::string& name) {
    for (const BuiltinDef& b : Builtins()) if (name == b.name) return &b;
    return nullptr;
}

static bool ComponentTypeMatches(const char* required, const std::string& objectKind) {
    if (!required || !*required) return true;
    if (objectKind.empty()) return true;
    return objectKind == required || ObjectKindRegistry::Get().IsCompatible(objectKind, required);
}

const MethodDef* FindMethod(Kind receiver, const std::string& name, const std::string& objectKind) {
    for (const MethodDef& m : Methods()) {
        if (m.receiver == receiver && name == m.name && ComponentTypeMatches(m.componentType, objectKind)) return &m;
    }
    return nullptr;
}

const PropertyDef* FindProperty(Kind receiver, const std::string& name, const std::string& objectKind) {
    for (const PropertyDef& p : Properties()) {
        if (p.receiver == receiver && name == p.name && ComponentTypeMatches(p.componentType, objectKind)) return &p;
    }
    return nullptr;
}

static std::string NormalizeIdentifier(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == ' ' || c == '_') continue;
        out += (char)tolower((unsigned char)c);
    }
    return out;
}

std::string ComponentFieldIdentifier(const ComponentFieldDesc& field) {
    std::string out;
    for (char c : field.label.empty() ? field.key : field.label) {
        if (c == ' ') out += '_';
        else out += (char)tolower((unsigned char)c);
    }
    return out;
}

const ComponentFieldDesc* FindComponentField(const std::string& componentType, const std::string& identifier) {
    const ComponentTypeInfo* info = ComponentRegistry::Get().Find(componentType);
    if (!info) return nullptr;
    std::string wanted = NormalizeIdentifier(identifier);
    for (const ComponentFieldDesc& f : info->fields) {
        if (NormalizeIdentifier(f.label) == wanted || NormalizeIdentifier(f.key) == wanted) return &f;
    }
    return nullptr;
}

SType ComponentFieldScriptType(const ComponentFieldDesc& field) {
    if (field.kind == ComponentFieldKind::Float) return MakeType(Kind::Float);
    if (field.kind == ComponentFieldKind::Bool) return MakeType(Kind::Bool);
    return MakeType(Kind::String);
}

static const ComponentFieldDesc* FindFieldByKey(const std::string& componentType, const std::string& key) {
    const ComponentTypeInfo* info = ComponentRegistry::Get().Find(componentType);
    if (!info) return nullptr;
    for (const ComponentFieldDesc& f : info->fields) if (f.key == key) return &f;
    return nullptr;
}

Value ReadComponentField(const Value& component, const std::string& fieldKey) {
    Component* c = RequireComponent(component, "component field");
    const ComponentFieldDesc* field = FindFieldByKey(c->GetClassName(), fieldKey);
    if (!field || !field->read) throw ScriptError{ "This component field cannot be read" };
    std::string text = field->read(*c);
    switch (field->kind) {
        case ComponentFieldKind::Float: return Value::Float(text.empty() ? 0.0 : strtod(text.c_str(), nullptr));
        case ComponentFieldKind::Bool: return Value::Bool(text == "true" || text == "1");
        default: return Value::String(text);
    }
}

void WriteComponentField(const Value& component, const std::string& fieldKey, const Value& value) {
    Component* c = RequireComponent(component, "component field");
    const ComponentFieldDesc* field = FindFieldByKey(c->GetClassName(), fieldKey);
    if (!field || !field->apply) throw ScriptError{ "This component field cannot be written" };
    ComponentDecl decl;
    switch (field->kind) {
        case ComponentFieldKind::Float: decl.fields[fieldKey] = ComponentField::FloatStr((float)value.AsNumber()); break;
        case ComponentFieldKind::Bool: decl.fields[fieldKey] = ComponentField::BoolStr(value.b); break;
        default: decl.fields[fieldKey] = value.s; break;
    }
    field->apply(*c, decl);
}

}
