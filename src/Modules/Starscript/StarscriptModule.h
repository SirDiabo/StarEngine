#pragma once
#include "Core/Module.h"

class StarscriptModule : public Module {
public:
    StarscriptModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};
