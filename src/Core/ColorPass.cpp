#include "ColorPass.h"
#include "Runtime/SceneRenderer.h"
#include "ShadingModel.h"

namespace ColorPass {

void Run(Camera3D camera) {
    ShadingModel::UpdateLightUniforms();
    SceneRenderer::DrawWorldActors(camera);
}

}