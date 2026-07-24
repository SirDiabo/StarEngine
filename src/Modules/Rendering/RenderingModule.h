#pragma once
#include "Core/Module.h"

class RenderingModule : public Module {
public:
    RenderingModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};
