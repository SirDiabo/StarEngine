#pragma once
#include "raylib.h"

class Actor;

namespace CollisionTrace {
    bool TraceAgainstMeshes(const Vector3& start, const Vector3& end, Actor* exclude,
                             Actor*& outActor, Vector3& outLocation, Vector3& outNormal, float& outDistance);
}
