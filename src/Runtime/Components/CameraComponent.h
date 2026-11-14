#pragma once
#include "SceneComponent.h"

class CameraComponent : public SceneComponent {
public:
    float fov = 50.0f;
    bool orthographic = false;
    float nearClip = 5.0f;
    float farClip = 400000.0f;

    bool useControlYaw = false;
    bool useControlPitch = true;
    bool useControlRoll = false;

    CameraComponent();
    const char* GetClassName() const override { return "CameraComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};

