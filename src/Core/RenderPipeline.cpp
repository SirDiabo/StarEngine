#include "RenderPipeline.h"
#include "ColorPass.h"
#include "PostProcessPass.h"
#include "ShadingTechniqueRegistry.h"
#include "RenderSettings.h"

namespace RenderPipeline {

void Render(Camera3D camera, RenderTexture2D target, Color clearColor, bool showPlaceholders) {
    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(RenderSettings::Get().GetShadingType());

    BeginTextureMode(target);
    ClearBackground(clearColor);

    BeginMode3D(camera);
    ColorPass::Run(camera, showPlaceholders);
    if (activeTechnique && activeTechnique->renderInWorld) activeTechnique->renderInWorld(camera, target);
    EndMode3D();

    if (activeTechnique && activeTechnique->renderOverlay) activeTechnique->renderOverlay(target);
    if (PostProcessPass::Enabled()) PostProcessPass::Run(target);

    EndTextureMode();
}

}
