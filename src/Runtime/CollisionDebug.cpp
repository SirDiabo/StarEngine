#include "CollisionDebug.h"
#include "DebugDraw.h"
#include <vector>

namespace {

struct DebugLine { Vector3 start, end; Color color; };
struct DebugSphere { Vector3 center; float radius; Color color; };

std::vector<DebugLine> g_DebugLines;
std::vector<DebugSphere> g_DebugSpheres;
bool s_CollisionDebugEnabled = false;

}

void SetCollisionDebugEnabled(bool enabled) { s_CollisionDebugEnabled = enabled; }
bool IsCollisionDebugEnabled() { return s_CollisionDebugEnabled; }

void AddCollisionDebugLine(Vector3 start, Vector3 end, Color color) {
    g_DebugLines.push_back({ start, end, color });
}

void AddCollisionDebugSphere(Vector3 center, float radius, Color color) {
    g_DebugSpheres.push_back({ center, radius, color });
}

void DrawAndClearCollisionDebug(float viewportHeight) {
    if (viewportHeight > 1.0f) DebugDraw::viewportHeight = viewportHeight;
    if (s_CollisionDebugEnabled) {
        const DebugDraw::View view = DebugDraw::CaptureView();
        const float width = DebugDraw::lineTraceLineWidth;
        for (const auto& line : g_DebugLines) {
            DebugDraw::Line(view, line.start, line.end, width, line.color);
        }
        for (const auto& sphere : g_DebugSpheres) {
            DebugDraw::Sphere(view, sphere.center, sphere.radius, width, sphere.color);
        }
    }
    g_DebugLines.clear();
    g_DebugSpheres.clear();
}
