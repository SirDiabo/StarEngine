#include "RenderPipeline.h"
#include "Passes/ShadowPass.h"
#include "Passes/ColorPass.h"
#include "Passes/PostProcessPass.h"
#include "Core/Module.h"

namespace RenderPipeline {

void Render(Camera3D camera, RenderTexture2D target, Color clearColor, bool showPlaceholders) {
    BeginTextureMode(target);
    ClearBackground(clearColor);

    bool renderingEnabled = ModuleManager::Get().IsModuleEnabled("Rendering");

    BeginMode3D(camera);
    ColorPass::Run(camera, showPlaceholders);
    if (renderingEnabled && ShadowPass::Enabled()) ShadowPass::Run();
    EndMode3D();

    if (renderingEnabled && PostProcessPass::Enabled()) PostProcessPass::Run(target);

    EndTextureMode();
}

}