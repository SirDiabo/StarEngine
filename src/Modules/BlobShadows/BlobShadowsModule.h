#pragma once
#include "Core/Module.h"

class BlobShadowsModule : public Module {
public:
    BlobShadowsModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};
