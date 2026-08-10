#pragma once
#include "raylib.h"

namespace RenderPipeline {
    void Render(Camera3D camera, RenderTexture2D target, Color clearColor, bool showPlaceholders);
}
