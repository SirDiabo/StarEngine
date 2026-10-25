#include "InputComponent.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/ComponentRegistry.h"
#include "raylib.h"
#include <cstdio>

void InputComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

namespace {
ComponentRegistrar registrar_InputComponent({
    "InputComponent",
    []() -> Component* { return new InputComponent(); },
    {
        ComponentField::Dev("InputDevice", "Device",
            [](Component& c, const ComponentDecl& d) { static_cast<InputComponent&>(c).device = d.Get("InputDevice", "Unclaimed"); }, [](const Component& c) { return static_cast<const InputComponent&>(c).device; }),
    },
    [](Component& c, const ComponentDecl& d) {
        auto it = d.arrayFields.find("InputMapping");
        if (it != d.arrayFields.end()) static_cast<InputComponent&>(c).mappingNames = it->second;
    },
    [](const Component& c, ComponentDecl& d) { d.arrayFields["InputMapping"] = static_cast<const InputComponent&>(c).mappingNames; },
    { "InputMapping" }
});
}