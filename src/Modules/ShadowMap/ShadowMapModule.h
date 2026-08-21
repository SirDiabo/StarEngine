#pragma once
#include "Core/Module.h"

class ShadowMapModule : public Module {
public:
    ShadowMapModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};
