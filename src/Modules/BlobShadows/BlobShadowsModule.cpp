#include "BlobShadowsModule.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "Passes/ShadowPass.h"

#ifndef STARENGINE_GAME_BUILD
#include "Editor/BlobShadowsSettingsUI.h"
#endif

BlobShadowsModule::BlobShadowsModule() {
    name = "BlobShadows";
}

bool BlobShadowsModule::Initialize() {
    ShadingTechnique technique;
    technique.id = "ProjectedBlobShadows";
    technique.displayName = "Blob Shadows";
    technique.moduleName = "BlobShadows";
    technique.renderInWorld = [](Camera3D, RenderTexture2D) { ShadowPass::Run(); };
#ifndef STARENGINE_GAME_BUILD
    technique.renderSettings = BlobShadowsSettingsUI::Render;
#endif
    ShadingTechniqueRegistry::Get().Register(technique);
    return true;
}

void BlobShadowsModule::Shutdown() {}
void BlobShadowsModule::Update(float deltaTime) {}
