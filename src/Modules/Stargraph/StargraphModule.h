#pragma once
#include "Core/Module.h"

class StargraphModule : public Module {
public:
    StargraphModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

private:
    void RegisterDefaultNodes();
};
