#pragma once
#include "raylib.h"

namespace ShadingModel {
    bool ShouldUseLitShading();

    Shader& GetLitShader();
    Shader GetDefaultShader();

    void UpdateLightUniforms();

    const char* GetLightingGLSL();
}