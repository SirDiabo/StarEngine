#pragma once
#include "raylib.h"
#include "rlgl.h"
#include <cstdlib>

namespace MeshInstancing {

inline bool DisabledByEnvironment() {
#ifdef _MSC_VER
#pragma warning(suppress : 4996)
#endif
    static const bool disabled = std::getenv("STARENGINE_NO_INSTANCING") != nullptr;
    return disabled;
}

inline bool Supported() {
    if (DisabledByEnvironment()) return false;
    const int version = rlGetVersion();
    return version == RL_OPENGL_33 || version == RL_OPENGL_43;
}

inline void SetShaderInstancing(const Shader& shader, bool enabled) {
    const int loc = GetShaderLocation(shader, "useInstancing");
    if (loc < 0) return;
    const int value = enabled ? 1 : 0;
    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_INT);
}

inline void Draw(const Mesh& mesh, const Material& material, const Matrix* transforms, int count) {
    const int attribLoc = material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM];
    if (attribLoc < 0 || count <= 0 || mesh.vaoId == 0) return;

    SetShaderInstancing(material.shader, true);
    DrawMeshInstanced(mesh, material, transforms, count);
    SetShaderInstancing(material.shader, false);

    if (rlEnableVertexArray(mesh.vaoId)) {
        for (int i = 0; i < 4; i++) rlDisableVertexAttribute((unsigned int)(attribLoc + i));
        rlDisableVertexArray();
    }
}

}
