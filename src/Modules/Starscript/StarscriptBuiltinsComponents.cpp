#include "StarscriptBuiltins.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Components/Component.h"
#include "Runtime/ComponentRegistry.h"
#include "Runtime/Components/PhysicsComponent.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/InputComponent.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "raylib.h"
#include <string>

namespace Starscript {

namespace {

SType T(Kind k) { return MakeType(k); }
Value V3(const Vector3& v) { return Value::Vec3(v.x, v.y, v.z); }
Vector3 ToV3(const Value& v) { return { v.x, v.y, v.z }; }

Component* RequireComp(const Value& v, const char* what) {
    Component* c = ResolveComponent(v);
    if (!c) throw ScriptError{ std::string(what) + ": the component is not valid" };
    return c;
}

PhysicsComponent* RequirePhysicsComp(const Value& v, const char* what) {
    PhysicsComponent* p = dynamic_cast<PhysicsComponent*>(RequireComp(v, what));
    if (!p) throw ScriptError{ std::string(what) + ": this is not a PhysicsComponent" };
    return p;
}

MeshComponent* RequireMeshComp(const Value& v, const char* what) {
    MeshComponent* m = dynamic_cast<MeshComponent*>(RequireComp(v, what));
    if (!m) throw ScriptError{ std::string(what) + ": this is not a MeshComponent" };
    return m;
}

InputComponent* RequireInputComp(const Value& v, const char* what) {
    InputComponent* i = dynamic_cast<InputComponent*>(RequireComp(v, what));
    if (!i) throw ScriptError{ std::string(what) + ": this is not an InputComponent" };
    return i;
}

void WriteByIdentifier(const Value& self, const char* identifier, const Value& value, const char* what) {
    Component* c = RequireComp(self, what);
    const ComponentFieldDesc* field = FindComponentField(c->GetClassName(), identifier);
    if (!field) throw ScriptError{ std::string(what) + ": this component has no '" + identifier + "' setting" };
    WriteComponentField(self, field->key, value);
}

Value ReadByIdentifier(const Value& self, const char* identifier, const char* what) {
    Component* c = RequireComp(self, what);
    const ComponentFieldDesc* field = FindComponentField(c->GetClassName(), identifier);
    if (!field) throw ScriptError{ std::string(what) + ": this component has no '" + identifier + "' setting" };
    return ReadComponentField(self, field->key);
}

Value M_phys_set_velocity(Runtime&, Value& s, std::vector<Value>& a) { RequirePhysicsComp(s, "set_velocity")->SetVelocity(ToV3(a[0])); return Value::Void(); }
Value M_phys_get_velocity(Runtime&, Value& s, std::vector<Value>&) { return V3(RequirePhysicsComp(s, "get_velocity")->velocity); }
Value M_phys_is_grounded(Runtime&, Value& s, std::vector<Value>&) { return Value::Bool(RequirePhysicsComp(s, "is_grounded")->grounded); }
Value M_phys_ground_normal(Runtime&, Value& s, std::vector<Value>&) { return V3(RequirePhysicsComp(s, "get_ground_normal")->groundNormal); }
Value M_phys_set_simulate(Runtime&, Value& s, std::vector<Value>& a) { RequirePhysicsComp(s, "set_simulate_physics")->simulatePhysics = a[0].b; return Value::Void(); }

Value M_mesh_set_static_mesh(Runtime&, Value& s, std::vector<Value>& a) { RequireMeshComp(s, "set_static_mesh")->SetMeshPath(a[0].s); return Value::Void(); }
Value M_mesh_set_material(Runtime&, Value& s, std::vector<Value>& a) {
    int slot = a.size() > 1 ? (int)a[1].i : 0;
    if (slot < 0) throw ScriptError{ "set_material: the material slot cannot be negative" };
    RequireMeshComp(s, "set_material")->SetMaterialPath(slot, a[0].s);
    return Value::Void();
}

Value M_audio_set_sound_path(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "sound_path", a[0], "set_sound_path"); return Value::Void(); }
Value M_audio_set_volume(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "volume", a[0], "set_volume"); return Value::Void(); }
Value M_audio_set_pitch(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "pitch", a[0], "set_pitch"); return Value::Void(); }
Value M_audio_set_looping(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "looping", a[0], "set_looping"); return Value::Void(); }

Value M_camera_set_fov(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "fov", a[0], "set_fov"); return Value::Void(); }
Value M_camera_get_fov(Runtime&, Value& s, std::vector<Value>&) { return ReadByIdentifier(s, "fov", "get_fov"); }
Value M_camera_set_orthographic(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "orthographic", a[0], "set_orthographic"); return Value::Void(); }
Value M_camera_is_orthographic(Runtime&, Value& s, std::vector<Value>&) { return ReadByIdentifier(s, "orthographic", "is_orthographic"); }
Value M_camera_set_clip(Runtime&, Value& s, std::vector<Value>& a) {
    WriteByIdentifier(s, "near_clip", a[0], "set_clip");
    WriteByIdentifier(s, "far_clip", a[1], "set_clip");
    return Value::Void();
}
Value M_camera_make_active(Runtime&, Value& s, std::vector<Value>&) {
    Component* c = RequireComp(s, "make_active");
    Actor* owner = c->GetOwningActor();
    if (!owner) throw ScriptError{ "make_active: the component has no owning actor" };
    World::Get().SetViewTarget(owner);
    return Value::Void();
}

Value M_collision_set_radius(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "radius", a[0], "set_radius"); return Value::Void(); }
Value M_collision_set_height(Runtime&, Value& s, std::vector<Value>& a) { WriteByIdentifier(s, "height", a[0], "set_height"); return Value::Void(); }

bool DeviceClaimedByOther(const std::string& device, InputComponent* self) {
    for (const auto& actorPtr : World::Get().GetActors()) {
        InputComponent* ic = actorPtr->GetInputComponent();
        if (ic && ic != self && ic->device == device) return true;
    }
    return false;
}

Value M_input_override(Runtime&, Value& s, std::vector<Value>& a) {
    RequireInputComp(s, "override_input");
    if (a[0].s.empty() || a[1].s.empty()) return Value::Void();
    InputMapping mapping;
    if (const InputMapping* existing = InputMappingRegistry::Get().Find(a[0].s)) mapping = *existing;
    InputMappingBinding binding;
    binding.key = a[1].s;
    mapping.bindings = { binding };
    InputMappingRegistry::Get().SetMappingRuntime(a[0].s, mapping);
    return Value::Void();
}

Value M_input_claim(Runtime&, Value& s, std::vector<Value>&) {
    InputComponent* target = RequireInputComp(s, "claim_input_device");

    if (target->device == "Unclaimed" && !DeviceClaimedByOther("Keyboard+Mouse", target)) {
        for (const std::string& key : GetInputKeyNames()) {
            if (key.rfind("GAMEPAD_", 0) == 0 || InputKeyIsAnalog(key)) continue;
            float axis = 0.0f;
            if (PollInputKey(key, axis)) { target->device = "Keyboard+Mouse"; break; }
        }
    }

    if (target->device == "Unclaimed") {
        for (int i = 0; i < 4 && target->device == "Unclaimed"; i++) {
            if (!IsGamepadAvailable(i)) continue;
            std::string candidate = "Gamepad " + std::to_string(i);
            if (DeviceClaimedByOther(candidate, target)) continue;
            for (const std::string& key : GetInputKeyNames()) {
                if (key.rfind("GAMEPAD_", 0) != 0 || InputKeyIsAnalog(key)) continue;
                float axis = 0.0f;
                if (PollInputKey(key, axis, i)) { target->device = candidate; break; }
            }
        }
    }
    return Value::Bool(target->device != "Unclaimed");
}

Value M_input_get_device(Runtime&, Value& s, std::vector<Value>&) { return Value::String(RequireInputComp(s, "get_device")->device); }

}

const std::vector<MethodDef>& ComponentMethods() {
    static const std::vector<MethodDef> table = {
        { Kind::Component, "set_velocity", { T(Kind::Vec3) }, 1, T(Kind::Void), RetRule::Fixed, false, M_phys_set_velocity, "PhysicsComponent" },
        { Kind::Component, "get_velocity", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, M_phys_get_velocity, "PhysicsComponent" },
        { Kind::Component, "is_grounded", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_phys_is_grounded, "PhysicsComponent" },
        { Kind::Component, "get_ground_normal", {}, 0, T(Kind::Vec3), RetRule::Fixed, false, M_phys_ground_normal, "PhysicsComponent" },
        { Kind::Component, "set_simulate_physics", { T(Kind::Bool) }, 1, T(Kind::Void), RetRule::Fixed, false, M_phys_set_simulate, "PhysicsComponent" },

        { Kind::Component, "set_static_mesh", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, M_mesh_set_static_mesh, "MeshComponent" },
        { Kind::Component, "set_material", { T(Kind::String), T(Kind::Int) }, 1, T(Kind::Void), RetRule::Fixed, false, M_mesh_set_material, "MeshComponent" },

        { Kind::Component, "set_sound_path", { T(Kind::String) }, 1, T(Kind::Void), RetRule::Fixed, false, M_audio_set_sound_path, "AudioComponent" },
        { Kind::Component, "set_volume", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_audio_set_volume, "AudioComponent" },
        { Kind::Component, "set_pitch", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_audio_set_pitch, "AudioComponent" },
        { Kind::Component, "set_looping", { T(Kind::Bool) }, 1, T(Kind::Void), RetRule::Fixed, false, M_audio_set_looping, "AudioComponent" },

        { Kind::Component, "set_fov", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_camera_set_fov, "CameraComponent" },
        { Kind::Component, "get_fov", {}, 0, T(Kind::Float), RetRule::Fixed, false, M_camera_get_fov, "CameraComponent" },
        { Kind::Component, "set_orthographic", { T(Kind::Bool) }, 1, T(Kind::Void), RetRule::Fixed, false, M_camera_set_orthographic, "CameraComponent" },
        { Kind::Component, "is_orthographic", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_camera_is_orthographic, "CameraComponent" },
        { Kind::Component, "set_clip", { T(Kind::Float), T(Kind::Float) }, 2, T(Kind::Void), RetRule::Fixed, false, M_camera_set_clip, "CameraComponent" },
        { Kind::Component, "make_active", {}, 0, T(Kind::Void), RetRule::Fixed, false, M_camera_make_active, "CameraComponent" },

        { Kind::Component, "set_radius", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_collision_set_radius, "CollisionComponent" },
        { Kind::Component, "set_height", { T(Kind::Float) }, 1, T(Kind::Void), RetRule::Fixed, false, M_collision_set_height, "CollisionComponent" },

        { Kind::Component, "override_input", { T(Kind::String), T(Kind::String) }, 2, T(Kind::Void), RetRule::Fixed, false, M_input_override, "InputComponent" },
        { Kind::Component, "claim_input_device", {}, 0, T(Kind::Bool), RetRule::Fixed, false, M_input_claim, "InputComponent" },
        { Kind::Component, "get_device", {}, 0, T(Kind::String), RetRule::Fixed, false, M_input_get_device, "InputComponent" },
    };
    return table;
}

}
