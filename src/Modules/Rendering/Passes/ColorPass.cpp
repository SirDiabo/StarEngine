#include "ColorPass.h"
#include "Runtime/SceneRenderer.h"

namespace ColorPass {

void Run(Camera3D camera, bool showPlaceholders) {
    SceneRenderer::DrawWorldActors(showPlaceholders);
}

}
