#pragma once
#include "Component.h"

class SceneComponent : public Component {
public:
    SceneComponent();
    void Update(float deltaTime) override;
};