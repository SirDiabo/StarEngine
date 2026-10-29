#include "StarscriptInput.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/InputComponent.h"
#include "Runtime/World.h"
#include <algorithm>

namespace Starscript {

namespace {

std::string UpperKey(const std::string& name) {
    std::string out = name;
    for (char& c : out) c = (char)toupper((unsigned char)c);
    return out;
}

bool IsKeyName(const std::string& name) {
    std::string upper = UpperKey(name);
    for (const std::string& k : GetInputKeyNames()) if (k == upper) return true;
    return false;
}

}

bool IsKnownInputName(const std::string& name) {
    if (InputMappingRegistry::Get().Find(name)) return true;
    return IsKeyName(name);
}

InputReading SampleInput(Actor* owner, const std::string& name, bool widgetScope) {
    InputReading reading;
    if (widgetScope) owner = World::Get().GetInputFocus();
    else if (!owner || owner != World::Get().GetInputFocus()) return reading;

    std::string device = "Keyboard+Mouse";
    if (owner) {
        if (InputComponent* ic = owner->GetInputComponent()) device = ic->device;
    }

    const InputMapping* mapping = InputMappingRegistry::Get().Find(name);
    if (mapping) {
        for (const InputMappingBinding& binding : mapping->bindings) {
            int gamepadIndex = 0;
            if (!InputKeyBelongsToDevice(binding.key, device, gamepadIndex)) continue;
            if (binding.requireCtrl && !IsCtrlKeyDown()) continue;
            if (binding.requireAlt && !IsAltKeyDown()) continue;
            if (binding.requireShift && !IsShiftKeyDown()) continue;

            float raw = 0.0f;
            if (!PollInputKey(binding.key, raw, gamepadIndex)) continue;

            if (mapping->type == InputMappingType::Bool) reading.held = true;
            else if (mapping->type == InputMappingType::Axis) reading.axis += raw * binding.scale;
            else if (binding.axis == 0) reading.x += raw * binding.scale;
            else reading.y += raw * binding.scale;
        }
        if (mapping->type == InputMappingType::Bool) reading.axis = reading.held ? 1.0f : 0.0f;
        if (mapping->type == InputMappingType::Axis) reading.held = reading.axis != 0.0f;
        if (mapping->type == InputMappingType::Axis2D) {
            reading.held = reading.x != 0.0f || reading.y != 0.0f;
            reading.axis = reading.held ? 1.0f : 0.0f;
        }
        return reading;
    }

    std::string key = UpperKey(name);
    int gamepadIndex = 0;
    if (!InputKeyBelongsToDevice(key, device, gamepadIndex)) return reading;
    float raw = 0.0f;
    if (PollInputKey(key, raw, gamepadIndex)) {
        reading.held = true;
        reading.axis = raw;
    }
    return reading;
}

}
