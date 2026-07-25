#include "ShadowPass.h"
#include "Core/RenderSettings.h"

namespace ShadowPass {

bool Enabled() {
    return RenderSettings::Get().GetShadingType() == ShadingType::ProjectedBlobShadows;
}

void Run() {}

}