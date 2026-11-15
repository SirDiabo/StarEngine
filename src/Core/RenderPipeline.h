#pragma once
#include "raylib.h"

namespace RenderPipeline {
    void RunShadingPrePass(Camera3D camera, Vector2 targetSize);
    void Render(Camera3D camera, RenderTexture2D target, Color clearColor);
}
