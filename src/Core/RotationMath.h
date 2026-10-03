#pragma once
#include "raylib.h"
#include "raymath.h"

inline Matrix ComposeRotationDegrees(float pitchDeg, float yawDeg, float rollDeg) {
    Matrix rz = MatrixRotateZ(DEG2RAD * rollDeg);
    Matrix rx = MatrixRotateX(DEG2RAD * pitchDeg);
    Matrix ry = MatrixRotateY(DEG2RAD * yawDeg);
    return MatrixMultiply(MatrixMultiply(rz, rx), ry);
}

inline Matrix ComposeRotationDegrees(const Vector3& pitchYawRoll) {
    return ComposeRotationDegrees(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z);
}
