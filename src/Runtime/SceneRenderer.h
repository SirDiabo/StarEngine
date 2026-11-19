#pragma once
#include "raylib.h"

namespace SceneRenderer {
    struct Stats {
        int drawn = 0;
        int culledByFrustum = 0;
        int culledByDistance = 0;
        int instanced = 0;
    };
    const Stats& GetLastStats();
    void DrawWorldActors(Camera3D camera);
}
