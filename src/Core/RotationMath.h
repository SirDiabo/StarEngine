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

inline Vector3 LookAtRotationDegrees(const Vector3& from, const Vector3& to) {
    Vector3 dir = { to.x - from.x, to.y - from.y, to.z - from.z };
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (!std::isfinite(length) || length < 1e-6f) return { 0.0f, 0.0f, 0.0f };
    float sinPitch = dir.y / length;
    if (sinPitch > 1.0f) sinPitch = 1.0f;
    if (sinPitch < -1.0f) sinPitch = -1.0f;
    float pitch = -std::asin(sinPitch) * RAD2DEG;
    float yaw = std::atan2(dir.x, dir.z) * RAD2DEG;
    return { WrapAngleDegrees(pitch), WrapAngleDegrees(yaw), 0.0f };
}
