#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>

inline Matrix ComposeRotationDegrees(float pitchDeg, float yawDeg, float rollDeg) {
    Matrix rz = MatrixRotateZ(DEG2RAD * rollDeg);
    Matrix rx = MatrixRotateX(DEG2RAD * pitchDeg);
    Matrix ry = MatrixRotateY(DEG2RAD * yawDeg);
    return MatrixMultiply(MatrixMultiply(rz, rx), ry);
}

inline Matrix ComposeRotationDegrees(const Vector3& pitchYawRoll) {
    return ComposeRotationDegrees(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z);
}

inline float WrapAngleDegrees(float degrees) {
    if (!std::isfinite(degrees)) return 0.0f;
    float wrapped = std::fmod(degrees, 360.0f);
    if (wrapped > 180.0f) wrapped -= 360.0f;
    else if (wrapped <= -180.0f) wrapped += 360.0f;
    return wrapped;
}

inline float AngleDeltaDegrees(float from, float to) {
    return WrapAngleDegrees(WrapAngleDegrees(to) - WrapAngleDegrees(from));
}

inline Vector3 WrapRotationDegrees(const Vector3& rotation) {
    return { WrapAngleDegrees(rotation.x), WrapAngleDegrees(rotation.y), WrapAngleDegrees(rotation.z) };
}
