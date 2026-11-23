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

inline Vector3 RotationForwardVector(const Vector3& pitchYawRoll) {
    Matrix m = ComposeRotationDegrees(pitchYawRoll);
    return { m.m8, m.m9, m.m10 };
}

inline Vector3 RotationRightVector(const Vector3& pitchYawRoll) {
    Matrix m = ComposeRotationDegrees(pitchYawRoll);
    return { -m.m0, -m.m1, -m.m2 };
}

inline Vector3 RotationUpVector(const Vector3& pitchYawRoll) {
    Matrix m = ComposeRotationDegrees(pitchYawRoll);
    return { m.m4, m.m5, m.m6 };
}

inline Vector3 RotationFromBasisDegrees(const Vector3& right, const Vector3& up, const Vector3& forward) {
    float m0 = -right.x, m1 = -right.y, m2 = -right.z;
    float m5 = up.y;
    float m8 = forward.x, m9 = forward.y, m10 = forward.z;
    float sinPitch = -m9;
    if (sinPitch > 1.0f) sinPitch = 1.0f;
    if (sinPitch < -1.0f) sinPitch = -1.0f;
    float pitch = std::asin(sinPitch) * RAD2DEG;
    float yaw, roll;
    if (std::sqrt(m8 * m8 + m10 * m10) >= 1e-4f) {
        yaw = std::atan2(m8, m10) * RAD2DEG;
        roll = std::atan2(m1, m5) * RAD2DEG;
    } else {
        yaw = 0.0f;
        roll = std::atan2(m9 > 0.0f ? -m2 : m2, m0) * RAD2DEG;
    }
    return { WrapAngleDegrees(pitch), WrapAngleDegrees(yaw), WrapAngleDegrees(roll) };
}

namespace RotationAxes {

struct Basis {
    Vector3 right;
    Vector3 up;
    Vector3 forward;
};

inline float Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vector3 Cross3(const Vector3& a, const Vector3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline bool IsFinite3(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

inline bool Normalize3(Vector3& v, float minLength) {
    if (!IsFinite3(v)) return false;
    float length = std::sqrt(Dot3(v, v));
    if (!std::isfinite(length) || length < minLength) return false;
    v = { v.x / length, v.y / length, v.z / length };
    return true;
}

inline bool PerpendicularTo(const Vector3& axis, const Vector3& hint, Vector3& out) {
    float d = Dot3(axis, hint);
    out = { hint.x - axis.x * d, hint.y - axis.y * d, hint.z - axis.z * d };
    return Normalize3(out, 1e-4f);
}

inline bool Adjusted(const Vector3& first, Vector3 second, Vector3& out) {
    if (!Normalize3(second, 1e-12f)) return false;
    return PerpendicularTo(first, second, out);
}

inline Basis FromForward(const Vector3& forward) {
    Vector3 right = Cross3(forward, { 0.0f, 1.0f, 0.0f });
    if (!Normalize3(right, 1e-4f)) PerpendicularTo(forward, { -1.0f, 0.0f, 0.0f }, right);
    return { right, Cross3(right, forward), forward };
}

inline Basis FromUp(const Vector3& up) {
    Vector3 forward;
    if (!PerpendicularTo(up, { 0.0f, 0.0f, 1.0f }, forward)) PerpendicularTo(up, { 0.0f, 1.0f, 0.0f }, forward);
    return { Cross3(forward, up), up, forward };
}

inline Basis FromRight(const Vector3& right) {
    Vector3 up;
    if (!PerpendicularTo(right, { 0.0f, 1.0f, 0.0f }, up)) PerpendicularTo(right, { 0.0f, 0.0f, 1.0f }, up);
    return { right, up, Cross3(up, right) };
}

inline Basis FromForwardUp(const Vector3& forward, const Vector3& upHint) {
    Vector3 up;
    if (!Adjusted(forward, upHint, up)) return FromForward(forward);
    return { Cross3(forward, up), up, forward };
}

inline Basis FromForwardRight(const Vector3& forward, const Vector3& rightHint) {
    Vector3 right;
    if (!Adjusted(forward, rightHint, right)) return FromForward(forward);
    return { right, Cross3(right, forward), forward };
}

inline Basis FromRightForward(const Vector3& right, const Vector3& forwardHint) {
    Vector3 forward;
    if (!Adjusted(right, forwardHint, forward)) return FromRight(right);
    return { right, Cross3(right, forward), forward };
}

inline Basis FromRightUp(const Vector3& right, const Vector3& upHint) {
    Vector3 up;
    if (!Adjusted(right, upHint, up)) return FromRight(right);
    return { right, up, Cross3(up, right) };
}

inline Basis FromUpRight(const Vector3& up, const Vector3& rightHint) {
    Vector3 right;
    if (!Adjusted(up, rightHint, right)) return FromUp(up);
    return { right, up, Cross3(up, right) };
}

inline Basis FromUpForward(const Vector3& up, const Vector3& forwardHint) {
    Vector3 forward;
    if (!Adjusted(up, forwardHint, forward)) return FromUp(up);
    return { Cross3(forward, up), up, forward };
}

inline Vector3 ToDegrees(const Basis& b) { return RotationFromBasisDegrees(b.right, b.up, b.forward); }

}

inline Vector3 MakeRotFromX(Vector3 right) {
    if (!RotationAxes::Normalize3(right, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromRight(right));
}

inline Vector3 MakeRotFromY(Vector3 up) {
    if (!RotationAxes::Normalize3(up, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromUp(up));
}

inline Vector3 MakeRotFromZ(Vector3 forward) {
    if (!RotationAxes::Normalize3(forward, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromForward(forward));
}

inline Vector3 MakeRotFromXY(Vector3 right, const Vector3& up) {
    if (!RotationAxes::Normalize3(right, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromRightUp(right, up));
}

inline Vector3 MakeRotFromXZ(Vector3 right, const Vector3& forward) {
    if (!RotationAxes::Normalize3(right, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromRightForward(right, forward));
}

inline Vector3 MakeRotFromYX(Vector3 up, const Vector3& right) {
    if (!RotationAxes::Normalize3(up, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromUpRight(up, right));
}

inline Vector3 MakeRotFromYZ(Vector3 up, const Vector3& forward) {
    if (!RotationAxes::Normalize3(up, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromUpForward(up, forward));
}

inline Vector3 MakeRotFromZX(Vector3 forward, const Vector3& right) {
    if (!RotationAxes::Normalize3(forward, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromForwardRight(forward, right));
}

inline Vector3 MakeRotFromZY(Vector3 forward, const Vector3& up) {
    if (!RotationAxes::Normalize3(forward, 1e-12f)) return { 0.0f, 0.0f, 0.0f };
    return RotationAxes::ToDegrees(RotationAxes::FromForwardUp(forward, up));
}
