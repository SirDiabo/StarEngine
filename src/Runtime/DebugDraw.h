#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace DebugDraw {

constexpr float kMinLineWidth = 1.0f;
constexpr float kMaxLineWidth = 16.0f;

inline float collisionShapeLineWidth = 3.0f;
inline float lineTraceLineWidth = 3.0f;
inline float viewportHeight = 720.0f;

inline float ClampWidth(float w) {
    return w < kMinLineWidth ? kMinLineWidth : (w > kMaxLineWidth ? kMaxLineWidth : w);
}

inline std::string SettingsPath() {
    std::string home;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf) {
        home = buf;
        free(buf);
    } else {
        home = ".";
    }
#else
    const char* h = getenv("HOME");
    home = h ? h : ".";
#endif
    std::filesystem::path dir = std::filesystem::path(home) / ".starengine";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "debugdraw.cfg").string();
}

inline void SaveSettings() {
    std::ofstream out(SettingsPath());
    if (!out.is_open()) return;
    out << "collisionShapeLineWidth=" << collisionShapeLineWidth << "\n";
    out << "lineTraceLineWidth=" << lineTraceLineWidth << "\n";
}

inline void LoadSettings() {
    std::ifstream in(SettingsPath());
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        float value = (float)atof(line.substr(eq + 1).c_str());
        if (key == "collisionShapeLineWidth") collisionShapeLineWidth = ClampWidth(value);
        else if (key == "lineTraceLineWidth") lineTraceLineWidth = ClampWidth(value);
    }
}

struct View {
    Vector3 position{ 0, 0, 0 };
    Vector3 forward{ 0, 0, -1 };
    float projScale = 1.0f;
    bool ortho = false;
    float height = 720.0f;
};

inline View CaptureView() {
    Matrix mv = rlGetMatrixModelview();
    Matrix proj = rlGetMatrixProjection();
    Matrix inv = MatrixInvert(mv);
    View v;
    v.position = Vector3{ inv.m12, inv.m13, inv.m14 };
    v.forward = Vector3Normalize(Vector3{ -inv.m8, -inv.m9, -inv.m10 });
    v.projScale = fabsf(proj.m5) > 1e-6f ? proj.m5 : 1.0f;
    v.ortho = fabsf(proj.m15 - 1.0f) < 1e-3f;
    v.height = viewportHeight > 1.0f ? viewportHeight : 720.0f;
    return v;
}

inline float HalfWidthAt(const View& v, const Vector3& p, float widthPx) {
    float worldPerPixel;
    if (v.ortho) {
        worldPerPixel = 2.0f / (v.projScale * v.height);
    } else {
        float depth = Vector3DotProduct(Vector3Subtract(p, v.position), v.forward);
        if (depth < 0.01f) depth = 0.01f;
        worldPerPixel = 2.0f * depth / (v.projScale * v.height);
    }
    return 0.5f * widthPx * worldPerPixel;
}

inline void Line(const View& v, const Vector3& a, const Vector3& b, float widthPx, Color color) {
    if (widthPx <= 1.0f) {
        DrawLine3D(a, b, color);
        return;
    }
    Vector3 dir = Vector3Subtract(b, a);
    if (Vector3DotProduct(dir, dir) < 1e-12f) return;
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    Vector3 viewDir = v.ortho ? v.forward : Vector3Subtract(mid, v.position);
    Vector3 side = Vector3CrossProduct(dir, viewDir);
    float sl = Vector3Length(side);
    if (sl < 1e-9f) {
        DrawLine3D(a, b, color);
        return;
    }
    side = Vector3Scale(side, 1.0f / sl);
    Vector3 sa = Vector3Scale(side, HalfWidthAt(v, a, widthPx));
    Vector3 sb = Vector3Scale(side, HalfWidthAt(v, b, widthPx));
    Vector3 p0 = Vector3Add(a, sa);
    Vector3 p1 = Vector3Subtract(a, sa);
    Vector3 p2 = Vector3Subtract(b, sb);
    Vector3 p3 = Vector3Add(b, sb);

    rlCheckRenderBatchLimit(12);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(p0.x, p0.y, p0.z); rlVertex3f(p1.x, p1.y, p1.z); rlVertex3f(p2.x, p2.y, p2.z);
    rlVertex3f(p0.x, p0.y, p0.z); rlVertex3f(p2.x, p2.y, p2.z); rlVertex3f(p3.x, p3.y, p3.z);
    rlVertex3f(p2.x, p2.y, p2.z); rlVertex3f(p1.x, p1.y, p1.z); rlVertex3f(p0.x, p0.y, p0.z);
    rlVertex3f(p3.x, p3.y, p3.z); rlVertex3f(p2.x, p2.y, p2.z); rlVertex3f(p0.x, p0.y, p0.z);
    rlEnd();
}

inline void Arc(const View& v, const Vector3& center, const Vector3& u, const Vector3& w, float radius,
                float startAngle, float endAngle, int segments, float widthPx, Color color) {
    Vector3 prev = Vector3Add(center, Vector3Add(Vector3Scale(u, cosf(startAngle) * radius),
                                                 Vector3Scale(w, sinf(startAngle) * radius)));
    for (int i = 1; i <= segments; i++) {
        float t = startAngle + (endAngle - startAngle) * ((float)i / (float)segments);
        Vector3 cur = Vector3Add(center, Vector3Add(Vector3Scale(u, cosf(t) * radius), Vector3Scale(w, sinf(t) * radius)));
        Line(v, prev, cur, widthPx, color);
        prev = cur;
    }
}

inline void Circle(const View& v, const Vector3& center, const Vector3& u, const Vector3& w, float radius,
                   int segments, float widthPx, Color color) {
    Arc(v, center, u, w, radius, 0.0f, 2.0f * PI, segments, widthPx, color);
}

inline void Sphere(const View& v, const Vector3& center, float radius, float widthPx, Color color) {
    Vector3 x{ 1, 0, 0 };
    Vector3 y{ 0, 1, 0 };
    Vector3 z{ 0, 0, 1 };
    Circle(v, center, x, y, radius, 20, widthPx, color);
    Circle(v, center, y, z, radius, 20, widthPx, color);
    Circle(v, center, z, x, radius, 20, widthPx, color);
}

inline void Capsule(const View& v, const Vector3& a, const Vector3& b, float radius, float widthPx, Color color) {
    Vector3 axis = Vector3Subtract(b, a);
    float len = Vector3Length(axis);
    if (len < 1e-6f) {
        Sphere(v, a, radius, widthPx, color);
        return;
    }
    Vector3 w = Vector3Scale(axis, 1.0f / len);
    Vector3 ref = fabsf(w.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(w, ref));
    Vector3 t = Vector3CrossProduct(w, u);

    Circle(v, a, u, t, radius, 20, widthPx, color);
    Circle(v, b, u, t, radius, 20, widthPx, color);

    Vector3 dirs[4] = { u, t, Vector3Negate(u), Vector3Negate(t) };
    for (int i = 0; i < 4; i++) {
        Vector3 off = Vector3Scale(dirs[i], radius);
        Line(v, Vector3Add(a, off), Vector3Add(b, off), widthPx, color);
    }

    Arc(v, b, u, w, radius, 0.0f, PI, 10, widthPx, color);
    Arc(v, b, t, w, radius, 0.0f, PI, 10, widthPx, color);
    Arc(v, a, u, w, radius, PI, 2.0f * PI, 10, widthPx, color);
    Arc(v, a, t, w, radius, PI, 2.0f * PI, 10, widthPx, color);
}

inline void OrientedBox(const View& v, const Vector3& center, const Vector3 axis[3], const Vector3& half,
                        float widthPx, Color color) {
    Vector3 c[8];
    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) ? 1.0f : -1.0f;
        float sy = (i & 2) ? 1.0f : -1.0f;
        float sz = (i & 4) ? 1.0f : -1.0f;
        c[i] = Vector3Add(center, Vector3Add(Vector3Add(Vector3Scale(axis[0], half.x * sx), Vector3Scale(axis[1], half.y * sy)),
                                             Vector3Scale(axis[2], half.z * sz)));
    }
    for (int i = 0; i < 8; i++) {
        for (int bit = 1; bit < 8; bit <<= 1) {
            int j = i ^ bit;
            if (j > i) Line(v, c[i], c[j], widthPx, color);
        }
    }
}

}
