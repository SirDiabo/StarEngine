#pragma once
#include "Core/Module.h"

class StarsequenceModule : public Module {
public:
    StarsequenceModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};
