#include "RenderPipeline.h"
#include "ColorPass.h"
#include "PostProcessPass.h"
#include "ShadingTechniqueRegistry.h"
#include "RenderSettings.h"
#include "ShadingModel.h"
#include "Widgets/WidgetRenderer.h"

namespace RenderPipeline {

void Render(Camera3D camera, RenderTexture2D target, Color clearColor) {
    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(RenderSettings::Get().GetShadingType());

    WidgetRenderer::PrepareWorldSpaceWidgets();

    ShadingModel::ResetShadowSource();
    if (activeTechnique && activeTechnique->renderPrePass) {
        activeTechnique->renderPrePass(camera, {(float)target.texture.width, (float)target.texture.height});
    }

    BeginTextureMode(target);
    ClearBackground(clearColor);

    BeginMode3D(camera);
    ColorPass::Run(camera);
    if (activeTechnique && activeTechnique->renderInWorld) activeTechnique->renderInWorld(camera, target);
    EndMode3D();

    if (activeTechnique && activeTechnique->renderOverlay) activeTechnique->renderOverlay(target);
    if (PostProcessPass::Enabled()) PostProcessPass::Run(target);

    WidgetRenderer::DrawScreenSpaceWidgets({0, 0, (float)target.texture.width, (float)target.texture.height}, false, RenderSettings::Get().GetRenderScale());

    EndTextureMode();
}

}
