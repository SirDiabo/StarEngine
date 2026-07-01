#pragma once
#include "Core/Module.h"

class MeshesModule : public Module {
public:
    MeshesModule();

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

private:
    void RegisterAssetType();
};
