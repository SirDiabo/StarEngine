#include "ColorPass.h"
#include "Runtime/SceneRenderer.h"
#include "Runtime/CollisionTrace.h"
#include "ShadingModel.h"
#include "Widgets/WidgetRenderer.h"

namespace ColorPass {

void Run(Camera3D camera) {
    ShadingModel::UpdateLightUniforms();
    SceneRenderer::DrawWorldActors(camera);
    CollisionTrace::DrawDebugShapes();
    WidgetRenderer::DrawWorldSpaceWidgets(camera);
}

}
