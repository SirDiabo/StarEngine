#include "ColorPass.h"
#include "Runtime/SceneRenderer.h"
#include "ShadingModel.h"
#include "Widgets/WidgetRenderer.h"

namespace ColorPass {

void Run(Camera3D camera) {
    ShadingModel::UpdateLightUniforms();
    SceneRenderer::DrawWorldActors(camera);
    WidgetRenderer::DrawWorldSpaceWidgets(camera);
}

}