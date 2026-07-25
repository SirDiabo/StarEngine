#include "ColorPass.h"
#include "Runtime/SceneRenderer.h"
#include "Modules/Rendering/ShadingModel.h"

namespace ColorPass {

void Run(Camera3D camera, bool showPlaceholders) {
    ShadingModel::UpdateLightUniforms();
    SceneRenderer::DrawWorldActors(showPlaceholders);
}

}