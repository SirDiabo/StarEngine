#pragma once
#include "CollisionTrace.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

namespace CollisionPrivate {

using CollisionTrace::Shape;

inline float Comp(const Vector3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }

inline Vector3 V3(float x, float y, float z) { return Vector3{ x, y, z }; }
inline Vector3 Add(const Vector3& a, const Vector3& b) { return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vector3 Sub(const Vector3& a, const Vector3& b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vector3 Mul(const Vector3& a, float s) { return Vector3{ a.x * s, a.y * s, a.z * s }; }
inline Vector3 Neg(const Vector3& a) { return Vector3{ -a.x, -a.y, -a.z }; }
inline float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vector3 Cross(const Vector3& a, const Vector3& b) {
    return Vector3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float LenSq(const Vector3& a) { return Dot(a, a); }
inline float Len(const Vector3& a) { return sqrtf(Dot(a, a)); }
inline Vector3 Norm(const Vector3& a) {
    float l = Len(a);
    return l > 1e-20f ? Mul(a, 1.0f / l) : Vector3{ 0, 0, 0 };
}
inline float Saturate(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float SafeInv(float d) {
    if (fabsf(d) < 1e-20f) d = d < 0.0f ? -1e-20f : 1e-20f;
    return 1.0f / d;
}
inline Vector3 SafeInv3(const Vector3& d) { return Vector3{ SafeInv(d.x), SafeInv(d.y), SafeInv(d.z) }; }

inline Vector3 TransformPoint(const Matrix& m, const Vector3& v) {
    return Vector3{
        m.m0 * v.x + m.m4 * v.y + m.m8 * v.z + m.m12,
        m.m1 * v.x + m.m5 * v.y + m.m9 * v.z + m.m13,
        m.m2 * v.x + m.m6 * v.y + m.m10 * v.z + m.m14 };
}
inline Vector3 TransformDir(const Matrix& m, const Vector3& v) {
    return Vector3{
        m.m0 * v.x + m.m4 * v.y + m.m8 * v.z,
        m.m1 * v.x + m.m5 * v.y + m.m9 * v.z,
        m.m2 * v.x + m.m6 * v.y + m.m10 * v.z };
}

struct AABB { Vector3 mn; Vector3 mx; };

inline AABB EmptyAABB() { return AABB{ { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f } }; }
inline void Grow(AABB& b, const Vector3& p) {
    b.mn = Vector3{ fminf(b.mn.x, p.x), fminf(b.mn.y, p.y), fminf(b.mn.z, p.z) };
    b.mx = Vector3{ fmaxf(b.mx.x, p.x), fmaxf(b.mx.y, p.y), fmaxf(b.mx.z, p.z) };
}
inline void Grow(AABB& b, const AABB& o) {
    b.mn = Vector3{ fminf(b.mn.x, o.mn.x), fminf(b.mn.y, o.mn.y), fminf(b.mn.z, o.mn.z) };
    b.mx = Vector3{ fmaxf(b.mx.x, o.mx.x), fmaxf(b.mx.y, o.mx.y), fmaxf(b.mx.z, o.mx.z) };
}
inline bool Overlaps(const AABB& a, const AABB& b) {
    return a.mn.x <= b.mx.x && a.mx.x >= b.mn.x &&
           a.mn.y <= b.mx.y && a.mx.y >= b.mn.y &&
           a.mn.z <= b.mx.z && a.mx.z >= b.mn.z;
}
inline AABB TransformAABB(const AABB& b, const Matrix& m) {
    AABB out = EmptyAABB();
    for (int i = 0; i < 8; i++) {
        Vector3 c{ (i & 1) ? b.mx.x : b.mn.x, (i & 2) ? b.mx.y : b.mn.y, (i & 4) ? b.mx.z : b.mn.z };
        Grow(out, TransformPoint(m, c));
    }
    return out;
}

struct Tri { Vector3 a, b, c; };

struct BVHNode {
    Vector3 mn;
    Vector3 mx;
    int32_t offset;
    uint16_t count;
    uint16_t axis;
};

struct BVH {
    std::vector<BVHNode> nodes;
};

inline uint32_t BuildBVHRange(BVH& out, const std::vector<AABB>& boxes, const std::vector<Vector3>& centroids,
                              std::vector<uint32_t>& order, uint32_t begin, uint32_t end, uint32_t leafSize) {
    int idx = (int)out.nodes.size();
    out.nodes.emplace_back();

    AABB b = EmptyAABB();
    AABB cb = EmptyAABB();
    for (uint32_t i = begin; i < end; i++) {
        uint32_t it = order[i];
        Grow(b, boxes[it]);
        Grow(cb, centroids[it]);
    }
    uint32_t count = end - begin;
    out.nodes[idx].mn = b.mn;
    out.nodes[idx].mx = b.mx;
    out.nodes[idx].offset = 0;
    out.nodes[idx].count = 0;
    out.nodes[idx].axis = 0;

    if (count <= leafSize) {
        out.nodes[idx].offset = (int32_t)begin;
        out.nodes[idx].count = (uint16_t)count;
        return (uint32_t)idx;
    }

    Vector3 ext = Sub(cb.mx, cb.mn);
    int axis = (ext.x >= ext.y && ext.x >= ext.z) ? 0 : (ext.y >= ext.z ? 1 : 2);
    float extent = Comp(ext, axis);
    uint32_t mid = begin + count / 2;

    if (extent > 1e-9f) {
        float split = Comp(cb.mn, axis) + extent * 0.5f;
        auto it = std::partition(order.begin() + begin, order.begin() + end,
                                 [&](uint32_t t) { return Comp(centroids[t], axis) < split; });
        uint32_t partMid = (uint32_t)(it - order.begin());
        uint32_t left = partMid - begin;
        if (left < count / 4 || (count - left) < count / 4) {
            std::nth_element(order.begin() + begin, order.begin() + mid, order.begin() + end,
                             [&](uint32_t x, uint32_t y) { return Comp(centroids[x], axis) < Comp(centroids[y], axis); });
        } else {
            mid = partMid;
        }
    }

    BuildBVHRange(out, boxes, centroids, order, begin, mid, leafSize);
    uint32_t right = BuildBVHRange(out, boxes, centroids, order, mid, end, leafSize);
    out.nodes[idx].offset = (int32_t)right;
    out.nodes[idx].axis = (uint16_t)axis;
    return (uint32_t)idx;
}

inline void BuildBVH(BVH& out, const std::vector<AABB>& boxes, std::vector<uint32_t>& order, uint32_t leafSize = 4) {
    out.nodes.clear();
    order.resize(boxes.size());
    std::iota(order.begin(), order.end(), 0u);
    if (boxes.empty()) return;
    std::vector<Vector3> centroids(boxes.size());
    for (size_t i = 0; i < boxes.size(); i++) {
        centroids[i] = Mul(Add(boxes[i].mn, boxes[i].mx), 0.5f);
    }
    out.nodes.reserve(boxes.size() * 2 / (leafSize > 1 ? leafSize / 2 : 1) + 1);
    BuildBVHRange(out, boxes, centroids, order, 0, (uint32_t)boxes.size(), leafSize);
}

template <class Fn>
inline void TraverseBVHBox(const BVH& bvh, const AABB& q, Fn&& leaf) {
    if (bvh.nodes.empty()) return;
    int stack[96];
    int sp = 0;
    int cur = 0;
    for (;;) {
        const BVHNode& n = bvh.nodes[cur];
        bool hit = n.mn.x <= q.mx.x && n.mx.x >= q.mn.x &&
                   n.mn.y <= q.mx.y && n.mx.y >= q.mn.y &&
                   n.mn.z <= q.mx.z && n.mx.z >= q.mn.z;
        if (hit) {
            if (n.count > 0) {
                if (leaf((uint32_t)n.offset, (uint32_t)n.count)) return;
            } else {
                if (sp < 96) stack[sp++] = n.offset;
                cur = cur + 1;
                continue;
            }
        }
        if (sp == 0) return;
        cur = stack[--sp];
    }
}

inline bool RaySlab(const Vector3& mn, const Vector3& mx, const Vector3& o, const Vector3& inv, float tMax, float& tEnter) {
    float t1 = (mn.x - o.x) * inv.x;
    float t2 = (mx.x - o.x) * inv.x;
    float tmin = fminf(t1, t2);
    float tmax = fmaxf(t1, t2);
    t1 = (mn.y - o.y) * inv.y;
    t2 = (mx.y - o.y) * inv.y;
    tmin = fmaxf(tmin, fminf(t1, t2));
    tmax = fminf(tmax, fmaxf(t1, t2));
    t1 = (mn.z - o.z) * inv.z;
    t2 = (mx.z - o.z) * inv.z;
    tmin = fmaxf(tmin, fminf(t1, t2));
    tmax = fminf(tmax, fmaxf(t1, t2));
    tEnter = tmin;
    return tmax >= fmaxf(tmin, 0.0f) && tmin <= tMax;
}

template <class Fn>
inline void TraverseBVHRay(const BVH& bvh, const Vector3& o, const Vector3& inv, const float& tMax, Fn&& leaf) {
    if (bvh.nodes.empty()) return;
    int stack[96];
    int sp = 0;
    int cur = 0;
    for (;;) {
        const BVHNode& n = bvh.nodes[cur];
        float tEnter;
        if (RaySlab(n.mn, n.mx, o, inv, tMax, tEnter)) {
            if (n.count > 0) {
                leaf((uint32_t)n.offset, (uint32_t)n.count);
            } else {
                int l = cur + 1;
                int r = n.offset;
                if (Comp(inv, n.axis) < 0.0f) std::swap(l, r);
                if (sp < 96) stack[sp++] = r;
                cur = l;
                continue;
            }
        }
        if (sp == 0) return;
        cur = stack[--sp];
    }
}

struct MeshBVH {
    BVH tree;
    std::vector<Tri> tris;
};

struct ModelBVH {
    std::vector<MeshBVH> meshes;
    AABB bounds = EmptyAABB();
    bool skinned = false;
};

std::shared_ptr<const ModelBVH> BuildModelBVH(const Model& model);

inline bool RayTriangle(const Vector3& o, const Vector3& d, const Tri& t, float tMax, float& outT) {
    Vector3 e1 = Sub(t.b, t.a);
    Vector3 e2 = Sub(t.c, t.a);
    Vector3 p = Cross(d, e2);
    float det = Dot(e1, p);
    if (det > -1e-12f && det < 1e-12f) return false;
    float invDet = 1.0f / det;
    Vector3 tv = Sub(o, t.a);
    float u = Dot(tv, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    Vector3 q = Cross(tv, e1);
    float v = Dot(d, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    float tt = Dot(e2, q) * invDet;
    if (tt < 0.0f || tt > tMax) return false;
    outT = tt;
    return true;
}

inline bool RayMesh(const MeshBVH& mesh, const Vector3& o, const Vector3& d, const Vector3& inv, float& tMax, uint32_t& outTri) {
    bool hit = false;
    TraverseBVHRay(mesh.tree, o, inv, tMax, [&](uint32_t first, uint32_t count) {
        for (uint32_t i = first; i < first + count; i++) {
            float t;
            if (RayTriangle(o, d, mesh.tris[i], tMax, t)) {
                tMax = t;
                outTri = i;
                hit = true;
            }
        }
    });
    return hit;
}

inline Vector3 TriNormal(const Tri& t) {
    return Norm(Cross(Sub(t.b, t.a), Sub(t.c, t.a)));
}

inline Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c) {
    Vector3 ab = Sub(b, a);
    Vector3 ac = Sub(c, a);
    Vector3 ap = Sub(p, a);
    float d1 = Dot(ab, ap);
    float d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    Vector3 bp = Sub(p, b);
    float d3 = Dot(ab, bp);
    float d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return Add(a, Mul(ab, v));
    }

    Vector3 cp = Sub(p, c);
    float d5 = Dot(ab, cp);
    float d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return Add(a, Mul(ac, w));
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return Add(b, Mul(Sub(c, b), w));
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return Add(a, Add(Mul(ab, v), Mul(ac, w)));
}

inline Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b) {
    Vector3 ab = Sub(b, a);
    float l2 = Dot(ab, ab);
    if (l2 < 1e-20f) return a;
    float t = Saturate(Dot(Sub(p, a), ab) / l2);
    return Add(a, Mul(ab, t));
}

inline float ClosestSegmentSegment(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2,
                                   Vector3& c1, Vector3& c2) {
    Vector3 d1 = Sub(q1, p1);
    Vector3 d2 = Sub(q2, p2);
    Vector3 r = Sub(p1, p2);
    float a = Dot(d1, d1);
    float e = Dot(d2, d2);
    float f = Dot(d2, r);
    const float eps = 1e-12f;
    float s = 0.0f;
    float t = 0.0f;
    if (a <= eps && e <= eps) {
        s = 0.0f;
        t = 0.0f;
    } else if (a <= eps) {
        s = 0.0f;
        t = Saturate(f / e);
    } else {
        float c = Dot(d1, r);
        if (e <= eps) {
            t = 0.0f;
            s = Saturate(-c / a);
        } else {
            float b = Dot(d1, d2);
            float denom = a * e - b * b;
            s = denom > eps ? Saturate((b * f - c * e) / denom) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = Saturate(-c / a);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = Saturate((b - c) / a);
            }
        }
    }
    c1 = Add(p1, Mul(d1, s));
    c2 = Add(p2, Mul(d2, t));
    return LenSq(Sub(c1, c2));
}

inline bool PointInTriangle(const Vector3& p, const Tri& t) {
    Vector3 v0 = Sub(t.c, t.a);
    Vector3 v1 = Sub(t.b, t.a);
    Vector3 v2 = Sub(p, t.a);
    float d00 = Dot(v0, v0);
    float d01 = Dot(v0, v1);
    float d02 = Dot(v0, v2);
    float d11 = Dot(v1, v1);
    float d12 = Dot(v1, v2);
    float den = d00 * d11 - d01 * d01;
    if (fabsf(den) < 1e-20f) return false;
    float inv = 1.0f / den;
    float u = (d11 * d02 - d01 * d12) * inv;
    float v = (d00 * d12 - d01 * d02) * inv;
    return u >= 0.0f && v >= 0.0f && u + v <= 1.0f;
}

struct Contact {
    Vector3 normal{ 0, 1, 0 };
    Vector3 point{ 0, 0, 0 };
    float depth = 0.0f;
};

inline bool SphereTriangle(const Vector3& c, float r, const Tri& t, Contact& out) {
    Vector3 cp = ClosestPointOnTriangle(c, t.a, t.b, t.c);
    Vector3 d = Sub(c, cp);
    float d2 = LenSq(d);
    if (d2 > r * r) return false;
    float dist = sqrtf(d2);
    if (dist > 1e-6f) {
        out.normal = Mul(d, 1.0f / dist);
        out.depth = r - dist;
    } else {
        out.normal = TriNormal(t);
        out.depth = r;
    }
    out.point = cp;
    return true;
}

inline bool CapsuleTriangle(const Vector3& p0, const Vector3& p1, float r, const Tri& t, Contact& out) {
    Vector3 n = TriNormal(t);
    float bestD2 = 1e30f;
    Vector3 bestSeg{ 0, 0, 0 };
    Vector3 bestTri{ 0, 0, 0 };

    auto consider = [&](const Vector3& s, const Vector3& q) {
        float d2 = LenSq(Sub(s, q));
        if (d2 < bestD2) {
            bestD2 = d2;
            bestSeg = s;
            bestTri = q;
        }
    };

    consider(p0, ClosestPointOnTriangle(p0, t.a, t.b, t.c));
    consider(p1, ClosestPointOnTriangle(p1, t.a, t.b, t.c));

    Vector3 e[4] = { t.a, t.b, t.c, t.a };
    for (int i = 0; i < 3; i++) {
        Vector3 c1, c2;
        ClosestSegmentSegment(p0, p1, e[i], e[i + 1], c1, c2);
        consider(c1, c2);
    }

    if (LenSq(n) > 0.5f) {
        float sd0 = Dot(n, Sub(p0, t.a));
        float sd1 = Dot(n, Sub(p1, t.a));
        if (sd0 * sd1 < 0.0f) {
            float tt = sd0 / (sd0 - sd1);
            Vector3 x = Add(p0, Mul(Sub(p1, p0), tt));
            if (PointInTriangle(x, t)) {
                float a0 = fabsf(sd0);
                float a1 = fabsf(sd1);
                float sgn = a0 >= a1 ? (sd0 >= 0.0f ? 1.0f : -1.0f) : (sd1 >= 0.0f ? 1.0f : -1.0f);
                out.normal = Mul(n, sgn);
                out.depth = r + fminf(a0, a1);
                out.point = x;
                return true;
            }
        }
    }

    if (bestD2 > r * r) return false;
    float dist = sqrtf(bestD2);
    Vector3 d = Sub(bestSeg, bestTri);
    if (dist > 1e-6f) {
        out.normal = Mul(d, 1.0f / dist);
    } else {
        Vector3 mid = Mul(Add(p0, p1), 0.5f);
        out.normal = Dot(n, Sub(mid, t.a)) >= 0.0f ? n : Neg(n);
    }
    out.depth = r - dist;
    out.point = bestTri;
    return true;
}

inline bool BoxTriangle(const Shape& box, const Tri& tri, Contact& out) {
    const Vector3* w[3] = { &tri.a, &tri.b, &tri.c };
    Vector3 v[3];
    for (int i = 0; i < 3; i++) {
        Vector3 d = Sub(*w[i], box.center);
        v[i] = Vector3{ Dot(d, box.axis[0]), Dot(d, box.axis[1]), Dot(d, box.axis[2]) };
    }
    Vector3 f[3] = { Sub(v[1], v[0]), Sub(v[2], v[1]), Sub(v[0], v[2]) };
    const Vector3& h = box.half;

    float bestDepth = 1e30f;
    Vector3 bestAxis{ 0, 1, 0 };

    auto testAxis = [&](Vector3 L) -> bool {
        float l2 = LenSq(L);
        if (l2 < 1e-12f) return true;
        L = Mul(L, 1.0f / sqrtf(l2));
        float p0 = Dot(L, v[0]);
        float p1 = Dot(L, v[1]);
        float p2 = Dot(L, v[2]);
        float tmin = fminf(p0, fminf(p1, p2));
        float tmax = fmaxf(p0, fmaxf(p1, p2));
        float r = h.x * fabsf(L.x) + h.y * fabsf(L.y) + h.z * fabsf(L.z);
        if (tmin > r || tmax < -r) return false;
        float sPos = tmax + r;
        float sNeg = r - tmin;
        if (sPos < sNeg) {
            if (sPos < bestDepth) {
                bestDepth = sPos;
                bestAxis = L;
            }
        } else if (sNeg < bestDepth) {
            bestDepth = sNeg;
            bestAxis = Neg(L);
        }
        return true;
    };

    if (!testAxis(Cross(f[0], f[1]))) return false;
    Vector3 ex[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    for (int i = 0; i < 3; i++) {
        if (!testAxis(ex[i])) return false;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!testAxis(Cross(ex[i], f[j]))) return false;
        }
    }

    out.normal = Norm(Add(Add(Mul(box.axis[0], bestAxis.x), Mul(box.axis[1], bestAxis.y)), Mul(box.axis[2], bestAxis.z)));
    out.depth = bestDepth;
    out.point = ClosestPointOnTriangle(box.center, tri.a, tri.b, tri.c);
    return true;
}

inline bool ShapeTriangle(const Shape& s, const Tri& t, Contact& out) {
    switch (s.type) {
        case Shape::Sphere: return SphereTriangle(s.center, s.radius, t, out);
        case Shape::Capsule: return CapsuleTriangle(s.a, s.b, s.radius, t, out);
        default: return BoxTriangle(s, t, out);
    }
}

inline bool SpherePoint(const Vector3& c, float r, const Vector3& p, float pr, Contact& out) {
    Vector3 d = Sub(c, p);
    float d2 = LenSq(d);
    float sum = r + pr;
    if (d2 > sum * sum) return false;
    float dist = sqrtf(d2);
    out.normal = dist > 1e-6f ? Mul(d, 1.0f / dist) : Vector3{ 0, 1, 0 };
    out.depth = sum - dist;
    out.point = Add(p, Mul(out.normal, pr));
    return true;
}

inline Vector3 BoxLocal(const Shape& box, const Vector3& p) {
    Vector3 d = Sub(p, box.center);
    return Vector3{ Dot(d, box.axis[0]), Dot(d, box.axis[1]), Dot(d, box.axis[2]) };
}

inline Vector3 BoxToWorld(const Shape& box, const Vector3& l) {
    return Add(box.center, Add(Add(Mul(box.axis[0], l.x), Mul(box.axis[1], l.y)), Mul(box.axis[2], l.z)));
}

inline Vector3 BoxDirToWorld(const Shape& box, const Vector3& l) {
    return Add(Add(Mul(box.axis[0], l.x), Mul(box.axis[1], l.y)), Mul(box.axis[2], l.z));
}

inline Vector3 ClampToBox(const Vector3& l, const Vector3& h) {
    return Vector3{ fminf(fmaxf(l.x, -h.x), h.x), fminf(fmaxf(l.y, -h.y), h.y), fminf(fmaxf(l.z, -h.z), h.z) };
}

inline bool SphereBox(const Vector3& c, float r, const Shape& box, Contact& out) {
    Vector3 l = BoxLocal(box, c);
    Vector3 q = ClampToBox(l, box.half);
    Vector3 diff = Sub(l, q);
    float d2 = LenSq(diff);
    if (d2 > 1e-12f) {
        if (d2 > r * r) return false;
        float dist = sqrtf(d2);
        out.normal = Norm(BoxDirToWorld(box, Mul(diff, 1.0f / dist)));
        out.depth = r - dist;
        out.point = BoxToWorld(box, q);
        return true;
    }
    float bestGap = 1e30f;
    int bestAxis = 0;
    float bestSign = 1.0f;
    for (int i = 0; i < 3; i++) {
        float li = Comp(l, i);
        float gap = Comp(box.half, i) - fabsf(li);
        if (gap < bestGap) {
            bestGap = gap;
            bestAxis = i;
            bestSign = li >= 0.0f ? 1.0f : -1.0f;
        }
    }
    out.normal = Mul(box.axis[bestAxis], bestSign);
    out.depth = bestGap + r;
    out.point = c;
    return true;
}

inline bool CapsuleBox(const Vector3& p0, const Vector3& p1, float r, const Shape& box, Contact& out) {
    Vector3 la = BoxLocal(box, p0);
    Vector3 lb = BoxLocal(box, p1);
    Vector3 ld = Sub(lb, la);
    const Vector3& h = box.half;

    auto dist2At = [&](float t) {
        Vector3 p = Add(la, Mul(ld, t));
        return LenSq(Sub(p, ClampToBox(p, h)));
    };

    float lo = 0.0f;
    float hi = 1.0f;
    for (int i = 0; i < 32; i++) {
        float m1 = lo + (hi - lo) / 3.0f;
        float m2 = hi - (hi - lo) / 3.0f;
        if (dist2At(m1) < dist2At(m2)) hi = m2; else lo = m1;
    }
    float bestT = (lo + hi) * 0.5f;
    float bestD2 = dist2At(bestT);
    if (dist2At(0.0f) < bestD2) { bestT = 0.0f; bestD2 = dist2At(0.0f); }
    if (dist2At(1.0f) < bestD2) { bestT = 1.0f; bestD2 = dist2At(1.0f); }

    if (bestD2 > 1e-10f) {
        if (bestD2 > r * r) return false;
        Vector3 p = Add(la, Mul(ld, bestT));
        Vector3 q = ClampToBox(p, h);
        float dist = sqrtf(bestD2);
        out.normal = Norm(BoxDirToWorld(box, Mul(Sub(p, q), 1.0f / dist)));
        out.depth = r - dist;
        out.point = BoxToWorld(box, q);
        return true;
    }

    Vector3 mid = Mul(Add(la, lb), 0.5f);
    Vector3 halfSeg = Mul(ld, 0.5f);
    float bestS = 1e30f;
    int bestAxis = 0;
    float bestSign = 1.0f;
    for (int i = 0; i < 3; i++) {
        float ext = fabsf(Comp(halfSeg, i)) + r;
        float sPos = Comp(h, i) + ext - Comp(mid, i);
        float sNeg = Comp(mid, i) + ext + Comp(h, i);
        if (sPos < bestS) { bestS = sPos; bestAxis = i; bestSign = 1.0f; }
        if (sNeg < bestS) { bestS = sNeg; bestAxis = i; bestSign = -1.0f; }
    }
    out.normal = Mul(box.axis[bestAxis], bestSign);
    out.depth = bestS;
    out.point = BoxToWorld(box, ClampToBox(mid, h));
    return true;
}

inline bool BoxBox(const Shape& A, const Shape& B, Contact& out) {
    Vector3 tw = Sub(A.center, B.center);
    float bestDepth = 1e30f;
    Vector3 bestAxis{ 0, 1, 0 };

    auto testAxis = [&](Vector3 L, bool cross) -> bool {
        float l2 = LenSq(L);
        if (l2 < 1e-10f) return true;
        L = Mul(L, 1.0f / sqrtf(l2));
        float ra = A.half.x * fabsf(Dot(L, A.axis[0])) + A.half.y * fabsf(Dot(L, A.axis[1])) + A.half.z * fabsf(Dot(L, A.axis[2]));
        float rb = B.half.x * fabsf(Dot(L, B.axis[0])) + B.half.y * fabsf(Dot(L, B.axis[1])) + B.half.z * fabsf(Dot(L, B.axis[2]));
        float dist = Dot(L, tw);
        float overlap = ra + rb - fabsf(dist);
        if (overlap < 0.0f) return false;
        float score = cross ? overlap * 1.05f + 1e-4f : overlap;
        if (score < bestDepth) {
            bestDepth = score;
            bestAxis = dist >= 0.0f ? L : Neg(L);
            out.depth = overlap;
        }
        return true;
    };

    for (int i = 0; i < 3; i++) if (!testAxis(A.axis[i], false)) return false;
    for (int i = 0; i < 3; i++) if (!testAxis(B.axis[i], false)) return false;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!testAxis(Cross(A.axis[i], B.axis[j]), true)) return false;
        }
    }

    out.normal = bestAxis;
    Vector3 lb = BoxLocal(B, A.center);
    out.point = BoxToWorld(B, ClampToBox(lb, B.half));
    return true;
}

inline bool ShapeShape(const Shape& A, const Shape& B, Contact& out);

inline bool ShapeShapeSwapped(const Shape& A, const Shape& B, Contact& out) {
    if (!ShapeShape(B, A, out)) return false;
    out.normal = Neg(out.normal);
    return true;
}

inline bool ShapeShape(const Shape& A, const Shape& B, Contact& out) {
    switch (A.type) {
        case Shape::Sphere:
            switch (B.type) {
                case Shape::Sphere: return SpherePoint(A.center, A.radius, B.center, B.radius, out);
                case Shape::Capsule:
                    return SpherePoint(A.center, A.radius, ClosestPointOnSegment(A.center, B.a, B.b), B.radius, out);
                default: return SphereBox(A.center, A.radius, B, out);
            }
        case Shape::Capsule:
            switch (B.type) {
                case Shape::Sphere: return ShapeShapeSwapped(A, B, out);
                case Shape::Capsule: {
                    Vector3 c1, c2;
                    ClosestSegmentSegment(A.a, A.b, B.a, B.b, c1, c2);
                    return SpherePoint(c1, A.radius, c2, B.radius, out);
                }
                default: return CapsuleBox(A.a, A.b, A.radius, B, out);
            }
        default:
            switch (B.type) {
                case Shape::Box: return BoxBox(A, B, out);
                default: return ShapeShapeSwapped(A, B, out);
            }
    }
}

inline AABB ShapeAABB(const Shape& s) {
    AABB b = EmptyAABB();
    switch (s.type) {
        case Shape::Sphere:
            b.mn = Vector3{ s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
            b.mx = Vector3{ s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
            break;
        case Shape::Capsule:
            Grow(b, s.a);
            Grow(b, s.b);
            b.mn = Vector3{ b.mn.x - s.radius, b.mn.y - s.radius, b.mn.z - s.radius };
            b.mx = Vector3{ b.mx.x + s.radius, b.mx.y + s.radius, b.mx.z + s.radius };
            break;
        default: {
            Vector3 e{
                fabsf(s.axis[0].x) * s.half.x + fabsf(s.axis[1].x) * s.half.y + fabsf(s.axis[2].x) * s.half.z,
                fabsf(s.axis[0].y) * s.half.x + fabsf(s.axis[1].y) * s.half.y + fabsf(s.axis[2].y) * s.half.z,
                fabsf(s.axis[0].z) * s.half.x + fabsf(s.axis[1].z) * s.half.y + fabsf(s.axis[2].z) * s.half.z };
            b.mn = Sub(s.center, e);
            b.mx = Add(s.center, e);
            break;
        }
    }
    return b;
}

inline Shape TranslateShape(const Shape& s, const Vector3& d) {
    Shape o = s;
    o.center = Add(s.center, d);
    o.a = Add(s.a, d);
    o.b = Add(s.b, d);
    return o;
}

inline float ShapeMinExtent(const Shape& s) {
    if (s.type == Shape::Box) return fmaxf(fminf(s.half.x, fminf(s.half.y, s.half.z)), 1e-3f);
    return fmaxf(s.radius, 1e-3f);
}

inline bool RaySphere(const Vector3& o, const Vector3& d, const Vector3& c, float r, float tMax, float& outT, Vector3& outN) {
    Vector3 m = Sub(o, c);
    float b = Dot(m, d);
    float cc = Dot(m, m) - r * r;
    if (cc <= 0.0f) {
        outT = 0.0f;
        outN = Neg(d);
        return true;
    }
    if (b > 0.0f) return false;
    float disc = b * b - cc;
    if (disc < 0.0f) return false;
    float t = -b - sqrtf(disc);
    if (t < 0.0f || t > tMax) return false;
    outT = t;
    outN = Norm(Sub(Add(o, Mul(d, t)), c));
    return true;
}

inline bool RayCapsule(const Vector3& o, const Vector3& d, const Vector3& a, const Vector3& b, float r, float tMax,
                       float& outT, Vector3& outN) {
    float bestT = tMax;
    Vector3 bestN{ 0, 1, 0 };
    bool hit = false;
    float t;
    Vector3 n;
    if (RaySphere(o, d, a, r, bestT, t, n)) { bestT = t; bestN = n; hit = true; }
    if (RaySphere(o, d, b, r, bestT, t, n)) { bestT = t; bestN = n; hit = true; }

    Vector3 axis = Sub(b, a);
    float L = Len(axis);
    if (L > 1e-6f) {
        Vector3 w = Mul(axis, 1.0f / L);
        Vector3 m = Sub(o, a);
        float md = Dot(m, w);
        float nd = Dot(d, w);
        Vector3 dperp = Sub(d, Mul(w, nd));
        Vector3 mperp = Sub(m, Mul(w, md));
        float A = Dot(dperp, dperp);
        float B = Dot(mperp, dperp);
        float C = Dot(mperp, mperp) - r * r;
        if (C <= 0.0f && md >= 0.0f && md <= L) {
            outT = 0.0f;
            outN = Neg(d);
            return true;
        }
        if (A > 1e-12f) {
            float disc = B * B - A * C;
            if (disc >= 0.0f) {
                float tt = (-B - sqrtf(disc)) / A;
                if (tt >= 0.0f && tt <= bestT) {
                    float y = md + tt * nd;
                    if (y >= 0.0f && y <= L) {
                        Vector3 perp = Sub(Add(m, Mul(d, tt)), Mul(w, y));
                        bestT = tt;
                        bestN = Norm(perp);
                        hit = true;
                    }
                }
            }
        }
    }
    if (!hit) return false;
    outT = bestT;
    outN = bestN;
    return true;
}

inline bool RayBox(const Vector3& o, const Vector3& d, const Shape& box, float tMax, float& outT, Vector3& outN) {
    Vector3 lo = BoxLocal(box, o);
    Vector3 ld{ Dot(d, box.axis[0]), Dot(d, box.axis[1]), Dot(d, box.axis[2]) };
    float tmin = 0.0f;
    float tmax = tMax;
    int hitAxis = -1;
    float hitSign = 1.0f;
    bool inside = true;
    for (int i = 0; i < 3; i++) {
        float oi = Comp(lo, i);
        float di = Comp(ld, i);
        float hi = Comp(box.half, i);
        if (fabsf(di) < 1e-12f) {
            if (oi < -hi || oi > hi) return false;
            continue;
        }
        float inv = 1.0f / di;
        float t1 = (-hi - oi) * inv;
        float t2 = (hi - oi) * inv;
        float sgn = -1.0f;
        if (t1 > t2) { std::swap(t1, t2); sgn = 1.0f; }
        if (t1 > tmin) {
            tmin = t1;
            hitAxis = i;
            hitSign = sgn;
            inside = false;
        }
        tmax = fminf(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (inside || hitAxis < 0) {
        outT = 0.0f;
        outN = Neg(d);
        return true;
    }
    outT = tmin;
    outN = Mul(box.axis[hitAxis], hitSign);
    return true;
}

inline bool RayShape(const Shape& s, const Vector3& o, const Vector3& d, float tMax, float& outT, Vector3& outN) {
    switch (s.type) {
        case Shape::Sphere: return RaySphere(o, d, s.center, s.radius, tMax, outT, outN);
        case Shape::Capsule: return RayCapsule(o, d, s.a, s.b, s.radius, tMax, outT, outN);
        default: return RayBox(o, d, s, tMax, outT, outN);
    }
}

}
