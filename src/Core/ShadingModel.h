#pragma once
#include "raylib.h"

namespace ShadingModel {
    bool ShouldUseLitShading();

    Shader& GetLitShader();
    Shader GetDefaultShader();

    void UpdateLightUniforms();

    const char* GetLightingGLSL();

    void ResetShadowSource();
    void SetShadowSource(unsigned int shadowDepthTextureId, Matrix lightViewProj, Vector3 lightDirToSurface, int shadowMapSize, float strength, int aaMethod);
    void SetShadowSourceCube(Vector3 lightPos, const unsigned int faceTextureIds[6], const Matrix faceViewProj[6], int shadowMapSize, float strength, int aaMethod);
}