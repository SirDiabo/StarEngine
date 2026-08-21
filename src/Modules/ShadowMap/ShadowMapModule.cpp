#include "ShadowMapModule.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "Passes/ShadowMapPrePass.h"

ShadowMapModule::ShadowMapModule() {
    name = "ShadowMap";
}

bool ShadowMapModule::Initialize() {
    ShadingTechnique technique;
    technique.id = "ShadowMap";
    technique.displayName = "Shadow Map";
    technique.moduleName = "ShadowMap";
    technique.renderPrePass = [](Camera3D camera, Vector2) { ShadowMapPrePass::Run(camera); };
    ShadingTechniqueRegistry::Get().Register(technique);
    return true;
}

void ShadowMapModule::Shutdown() {
    ShadowMapPrePass::Shutdown();
}

void ShadowMapModule::Update(float deltaTime) {}
