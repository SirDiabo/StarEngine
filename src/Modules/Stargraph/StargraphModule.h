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
    void RegisterAssetType();
    void RegisterEditorTabs();
    void RegisterObjectKinds();
};

void RegisterEventNodes();
void RegisterFlowNodes();
void RegisterMathNodes();
void RegisterActorNodes();
void RegisterComponentNodes();
void RegisterVariableNodes();
void RegisterSaveNodes();
void RegisterUtilityNodes();
void RegisterTimerNodes();