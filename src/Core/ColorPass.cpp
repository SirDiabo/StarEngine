#include "ColorPass.h"
#include "ProfilerRender.h"
#include "Runtime/SceneRenderer.h"
#include "Runtime/CollisionTrace.h"
#include "ShadingModel.h"
#include "Widgets/WidgetRenderer.h"

namespace ColorPass {

void Run(Camera3D camera) {
    {
        PROFILE_RENDER_SCOPE("Update Light Uniforms");
        ShadingModel::UpdateLightUniforms();
    }
    {
        PROFILE_SCOPE("Scene Actors");
        SceneRenderer::DrawWorldActors(camera);
    }
    {
        PROFILE_RENDER_SCOPE("Collision Debug Shapes");
        CollisionTrace::DrawDebugShapes();
    }
    {
        PROFILE_RENDER_SCOPE("World-Space Widget Quads");
        WidgetRenderer::DrawWorldSpaceWidgets(camera);
    }
}

}
