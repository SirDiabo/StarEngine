#pragma once
#include "Component.h"

class SceneComponent : public Component {
public:
    SceneComponent();
    const char* GetClassName() const override { return "SceneComponent"; }
    void Update(float deltaTime) override;
};