#pragma once
#include "raylib.h"

namespace PostProcessPass {
    bool Enabled();
    void Run(RenderTexture2D target);
}
