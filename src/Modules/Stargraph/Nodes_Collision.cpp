#include "Nodes_Collision.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "ObjectPinRef.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/CollisionTrace.h"
#include "raymath.h"
#include <cstdio>
#include <vector>

struct DebugLine { Vector3 start, end; Color color; };
struct DebugSphere { Vector3 center; float radius; Color color; };

static std::vector<DebugLine> g_DebugLines;
static std::vector<DebugSphere> g_DebugSpheres;
static bool s_CollisionDebugEnabled = false;

void SetCollisionDebugEnabled(bool enabled) { s_CollisionDebugEnabled = enabled; }
bool IsCollisionDebugEnabled() { return s_CollisionDebugEnabled; }

void DrawAndClearCollisionDebug() {
    if (s_CollisionDebugEnabled) {
        for (const auto& line : g_DebugLines) {
            DrawLine3D(line.start, line.end, line.color);
        }
        for (const auto& sphere : g_DebugSpheres) {
            DrawSphereWires(sphere.center, sphere.radius, 8, 8, sphere.color);
        }
        CollisionTrace::DrawDebugShapes();
    }
    g_DebugLines.clear();
    g_DebugSpheres.clear();
}

void RegisterCollisionNodes() {
    NodeTemplate lineTraceForMesh;
    lineTraceForMesh.type = "Line_Trace_For_Mesh";
    lineTraceForMesh.category = "Collision";
    lineTraceForMesh.moduleName = "Stargraph";
    lineTraceForMesh.createPins = [](Node* node) {
        node->inputs.push_back({"In",         PinType::Exec,    true,  false});
        node->inputs.push_back({"Start",      PinType::Vec3,    true,  false, 0, "0,0,0"});
        node->inputs.push_back({"End",     PinType::Vec3,    true,  false, 0, "0,0,100"});
        node->inputs.push_back({"IgnoreSelf", PinType::Boolean, true,  false, 0, "true"});
        node->inputs.push_back({"DrawDebug",  PinType::Boolean, true,  false, 0, "false"});
        node->outputs.push_back({"Out",              PinType::Exec,    false, true});
        node->outputs.push_back({"Did Overlap",      PinType::Boolean, false, true});
        node->outputs.push_back({"Overlap Object",   PinType::Object,  false, true, 0, "", false, "Actor"});
        node->outputs.push_back({"Overlap Location", PinType::Vec3,    false, true});
        node->outputs.push_back({"Overlap Normal",   PinType::Vec3,    false, true});
        node->outputs.push_back({"Distance",         PinType::Float,   false, true});
    };
    lineTraceForMesh.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Vector3 start = {0,0,0}, end = {0,0,100};
        bool ignoreSelf = true, drawDebug = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Start")      ReadVec3(p, start.x, start.y, start.z);
            if (p.name == "End")     ReadVec3(p, end.x, end.y, end.z);
            if (p.name == "IgnoreSelf") ignoreSelf = ReadBool(p);
            if (p.name == "DrawDebug")  drawDebug = ReadBool(p);
        }
        Actor* exclude = ignoreSelf ? ctx.actor : nullptr;

        Actor* hitActor = nullptr;
        Vector3 hitLoc = end, hitNormal = {0, 1, 0};
        float hitDist = Vector3Distance(start, end);
        CollisionTrace::QueryParams queryParams;
        queryParams.ignoreActor = exclude;
        queryParams.shapes = false;
        CollisionTrace::HitResult traceHit;
        bool hit = CollisionTrace::Raycast(start, end, queryParams, traceHit);
        if (hit) {
            hitActor = traceHit.actor;
            hitLoc = traceHit.point;
            hitNormal = traceHit.normal;
            hitDist = traceHit.distance;
        }

        for (Pin& p : node.outputs) {
            if (p.name == "Did Overlap") p.defaultValue = hit ? "true" : "false";
            else if (p.name == "Overlap Object") {
                if (hit) {
                    p.defaultValue = ObjectPinRef::Encode(hitActor->GetObjectKind(), hitActor->GetObjectId());
                } else {
                    p.defaultValue = "";
                }
            } else if (p.name == "Overlap Location") {
                char b[64]; snprintf(b, sizeof(b), "%.3f,%.3f,%.3f", hitLoc.x, hitLoc.y, hitLoc.z);
                p.defaultValue = b;
            } else if (p.name == "Overlap Normal") {
                char b[64]; snprintf(b, sizeof(b), "%.3f,%.3f,%.3f", hitNormal.x, hitNormal.y, hitNormal.z);
                p.defaultValue = b;
            } else if (p.name == "Distance") {
                char b[32]; snprintf(b, sizeof(b), "%.3f", hitDist);
                p.defaultValue = b;
            }
        }

        if (drawDebug && s_CollisionDebugEnabled) {
            Vector3 lineEnd = hit ? hitLoc : end;
            g_DebugLines.push_back({start, lineEnd, hit ? RED : GREEN});
            if (hit) {
                g_DebugSpheres.push_back({hitLoc, 3.0f, RED});
                g_DebugLines.push_back({hitLoc, Vector3Add(hitLoc, Vector3Scale(hitNormal, 20.0f)), BLUE});
            }
        }
    };
    NodeRegistry::Get().RegisterNode(lineTraceForMesh);
}