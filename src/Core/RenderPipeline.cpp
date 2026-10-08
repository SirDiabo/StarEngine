#include "RenderPipeline.h"
#include "ColorPass.h"
#include "PostProcessPass.h"
#include "ShadingTechniqueRegistry.h"
#include "RenderSettings.h"
#include "ShadingModel.h"
#include "Widgets/WidgetRenderer.h"
#include "Runtime/DebugDraw.h"
#include "ProfilerRender.h"

namespace RenderPipeline {

void Render(Camera3D camera, RenderTexture2D target, Color clearColor) {
    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(RenderSettings::Get().GetShadingType());

    PROFILE_SCOPE("Render Pipeline");

    DebugDraw::viewportHeight = (float)target.texture.height;

    {
        PROFILE_RENDER_SCOPE("Prepare World-Space Widgets");
        WidgetRenderer::PrepareWorldSpaceWidgets();
    }

    ShadingModel::ResetShadowSource();
    if (activeTechnique && activeTechnique->renderPrePass) {
        PROFILE_RENDER_SCOPE("Shading Pre-Pass");
        activeTechnique->renderPrePass(camera, {(float)target.texture.width, (float)target.texture.height});
    }

    BeginTextureMode(target);
    ClearBackground(clearColor);

    BeginMode3D(camera);
    {
        PROFILE_RENDER_SCOPE("Color Pass");
        ColorPass::Run(camera);
    }
    if (activeTechnique && activeTechnique->renderInWorld) {
        PROFILE_RENDER_SCOPE("Shading In-World");
        activeTechnique->renderInWorld(camera, target);
    }
    EndMode3D();

    if (activeTechnique && activeTechnique->renderOverlay) {
        PROFILE_RENDER_SCOPE("Shading Overlay");
        activeTechnique->renderOverlay(target);
    }
    if (PostProcessPass::Enabled()) {
        PROFILE_RENDER_SCOPE("Post Process");
        PostProcessPass::Run(target);
    }

    {
        PROFILE_RENDER_SCOPE("Screen-Space Widgets");
        WidgetRenderer::DrawScreenSpaceWidgets({0, 0, (float)target.texture.width, (float)target.texture.height}, false, RenderSettings::Get().GetRenderScale());
    }

    EndTextureMode();
}

}
