#include "CameraComponent.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/SafeParse.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "raylib.h"
#include "raymath.h"

CameraComponent::CameraComponent() : SceneComponent() {
}

void CameraComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddFloat("FOV", &fov);
    outProperties.AddFloat("Near Clip", &nearClip);
    outProperties.AddFloat("Far Clip", &farClip);
    outProperties.AddBool("Orthographic", &orthographic);
    outProperties.AddBool("Use Control Yaw", &useControlYaw);
    outProperties.AddBool("Use Control Pitch", &useControlPitch);
    outProperties.AddBool("Use Control Roll", &useControlRoll);
}

void CameraComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

namespace {
ComponentRegistrar registrar_CameraComponent({
    "CameraComponent",
    []() -> Component* { return new CameraComponent(); },
    {
        ComponentField::Flt("CameraFOV", "FOV", 50.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).fov = SafeParse::ToFloat(d.Get("CameraFOV", "50")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CameraComponent&>(c).fov); }),
        ComponentField::Flt("CameraNearClip", "Near Clip", 0.1f,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).nearClip = SafeParse::ToFloat(d.Get("CameraNearClip", "0.1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CameraComponent&>(c).nearClip); }),
        ComponentField::Flt("CameraFarClip", "Far Clip", 4000.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).farClip = SafeParse::ToFloat(d.Get("CameraFarClip", "4000")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const CameraComponent&>(c).farClip); }),
        ComponentField::Bln("CameraOrthographic", "Orthographic", false,
            [](Component& c, const ComponentDecl& d) { static_cast<CameraComponent&>(c).orthographic = d.Get("CameraOrthographic", "false") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const CameraComponent&>(c).orthographic); }),
    }
});
}
