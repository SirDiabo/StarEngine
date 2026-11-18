#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

namespace Culling {

struct Frustum {
    float planes[6][4];
    bool valid = false;
};

inline Frustum FromViewProjection(const Matrix& m) {
    Frustum f;
    const float row[4][4] = {
        { m.m0,  m.m4,  m.m8,  m.m12 },
        { m.m1,  m.m5,  m.m9,  m.m13 },
        { m.m2,  m.m6,  m.m10, m.m14 },
        { m.m3,  m.m7,  m.m11, m.m15 },
    };
    const float sign[6] = { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
    const int axis[6] = { 0, 0, 1, 1, 2, 2 };
    for (int i = 0; i < 6; i++) {
        float a = row[3][0] + sign[i] * row[axis[i]][0];
        float b = row[3][1] + sign[i] * row[axis[i]][1];
        float c = row[3][2] + sign[i] * row[axis[i]][2];
        float d = row[3][3] + sign[i] * row[axis[i]][3];
        float len = sqrtf(a * a + b * b + c * c);
        if (!(len > 1e-12f) || !std::isfinite(len) || !std::isfinite(d)) return Frustum{};
        f.planes[i][0] = a / len;
        f.planes[i][1] = b / len;
        f.planes[i][2] = c / len;
        f.planes[i][3] = d / len;
    }
    f.valid = true;
    return f;
}

inline Frustum FromGLState() {
    return FromViewProjection(MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
}

inline bool Intersects(const Frustum& f, const BoundingBox& box) {
    if (!f.valid) return true;
    for (int i = 0; i < 6; i++) {
        const float* p = f.planes[i];
        float x = p[0] >= 0.0f ? box.max.x : box.min.x;
        float y = p[1] >= 0.0f ? box.max.y : box.min.y;
        float z = p[2] >= 0.0f ? box.max.z : box.min.z;
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0.0f) return false;
    }
    return true;
}

inline bool IsValidBox(const BoundingBox& b) {
    return std::isfinite(b.min.x) && std::isfinite(b.min.y) && std::isfinite(b.min.z)
        && std::isfinite(b.max.x) && std::isfinite(b.max.y) && std::isfinite(b.max.z)
        && b.min.x <= b.max.x && b.min.y <= b.max.y && b.min.z <= b.max.z;
}

inline BoundingBox TransformBox(const BoundingBox& box, const Matrix& world) {
    BoundingBox out;
    out.min = { INFINITY, INFINITY, INFINITY };
    out.max = { -INFINITY, -INFINITY, -INFINITY };
    for (int i = 0; i < 8; i++) {
        Vector3 corner = {
            (i & 1) ? box.max.x : box.min.x,
            (i & 2) ? box.max.y : box.min.y,
            (i & 4) ? box.max.z : box.min.z,
        };
        Vector3 w = Vector3Transform(corner, world);
        out.min.x = fminf(out.min.x, w.x); out.min.y = fminf(out.min.y, w.y); out.min.z = fminf(out.min.z, w.z);
        out.max.x = fmaxf(out.max.x, w.x); out.max.y = fmaxf(out.max.y, w.y); out.max.z = fmaxf(out.max.z, w.z);
    }
    return out;
}

inline BoundingBox Inflate(const BoundingBox& box, float amount) {
    return { { box.min.x - amount, box.min.y - amount, box.min.z - amount },
             { box.max.x + amount, box.max.y + amount, box.max.z + amount } };
}

inline float DistanceSquaredToBox(Vector3 p, const BoundingBox& box) {
    float dx = fmaxf(fmaxf(box.min.x - p.x, 0.0f), p.x - box.max.x);
    float dy = fmaxf(fmaxf(box.min.y - p.y, 0.0f), p.y - box.max.y);
    float dz = fmaxf(fmaxf(box.min.z - p.z, 0.0f), p.z - box.max.z);
    return dx * dx + dy * dy + dz * dz;
}

inline bool BeyondDistance(Vector3 viewer, const BoundingBox& box, float maxDistance) {
    if (!(maxDistance > 0.0f)) return false;
    return DistanceSquaredToBox(viewer, box) > maxDistance * maxDistance;
}

}
