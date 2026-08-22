#include "ShadowMapModule.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "Core/Project.h"
#include "Passes/ShadowMapPrePass.h"
#include "ShadowMapSettings.h"

#ifndef STARENGINE_GAME_BUILD
#include "Editor/ShadowMapSettingsUI.h"
#endif

ShadowMapModule::ShadowMapModule() {
    name = "ShadowMap";
}

bool ShadowMapModule::Initialize() {
    ShadowMapSettings::Get().Load(Project::Get().GetRootPath());

    ShadingTechnique technique;
    technique.id = "ShadowMap";
    technique.displayName = "Shadow Map";
    technique.moduleName = "ShadowMap";
    technique.renderPrePass = [](Camera3D camera, Vector2) { ShadowMapPrePass::Run(camera); };
#ifndef STARENGINE_GAME_BUILD
    technique.renderSettings = ShadowMapSettingsUI::Render;
#endif
    ShadingTechniqueRegistry::Get().Register(technique);
    return true;
}

void ShadowMapModule::Shutdown() {
    ShadowMapPrePass::Shutdown();
}

void ShadowMapModule::Update(float deltaTime) {}
