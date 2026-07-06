#pragma once
#include "SceneComponent.h"

class CameraComponent : public SceneComponent {
public:
    float fov = 50.0f;

    CameraComponent();
    const char* GetClassName() const override { return "CameraComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};

void RegisterCameraComponentNodes();
