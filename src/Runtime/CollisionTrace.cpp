#include "CollisionTrace.h"
#include "Actor.h"
#include "World.h"
#include "AssetCache.h"
#include "Components/MeshComponent.h"
#include "raymath.h"
#include <cmath>

static Matrix ComponentWorldMatrix(Actor* actor, Component* comp) {
    Vector3 pos, rot, scl;
    actor->GetComponentWorldTransform(comp, pos, rot, scl);
    Matrix scaleM = MatrixScale(scl.x, scl.y, scl.z);
    Matrix rotM = MatrixRotateXYZ({ DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z });
    Matrix transM = MatrixTranslate(pos.x, pos.y, pos.z);
    return MatrixMultiply(MatrixMultiply(scaleM, rotM), transM);
}

static bool SegmentHitsBox(Vector3 start, Vector3 dir, float length, BoundingBox box) {
    float tmin = 0.0f, tmax = length;
    float s[3] = { start.x, start.y, start.z };
    float d[3] = { dir.x, dir.y, dir.z };
    float bmin[3] = { box.min.x, box.min.y, box.min.z };
    float bmax[3] = { box.max.x, box.max.y, box.max.z };
    for (int i = 0; i < 3; i++) {
        if (fabsf(d[i]) < 1e-8f) {
            if (s[i] < bmin[i] || s[i] > bmax[i]) return false;
            continue;
        }
        float invD = 1.0f / d[i];
        float t1 = (bmin[i] - s[i]) * invD;
        float t2 = (bmax[i] - s[i]) * invD;
        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
        tmin = fmaxf(tmin, t1);
        tmax = fminf(tmax, t2);
        if (tmin > tmax) return false;
    }
    return true;
}

bool CollisionTrace::TraceAgainstMeshes(const Vector3& start, const Vector3& end, Actor* exclude,
                                         Actor*& outActor, Vector3& outLocation, Vector3& outNormal, float& outDistance) {
    Vector3 diff = Vector3Subtract(end, start);
    float dist = Vector3Length(diff);
    if (dist < 0.001f) return false;
    Vector3 dir = Vector3Scale(diff, 1.0f / dist);
    Ray ray = { start, dir };

    bool hit = false;
    float bestDist = 1e30f;

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor || actor == exclude) continue;
        MeshComponent* mc = actor->GetMeshComponent();
        if (!mc) continue;
        Model* model = mc->GetCachedModel();
        if (!model || model->meshCount <= 0) continue;

        Matrix world = ComponentWorldMatrix(actor, mc);

        Matrix inv = MatrixInvert(world);
        Vector3 localStart = Vector3Transform(start, inv);
        Vector3 localEnd = Vector3Transform(end, inv);
        Vector3 localDiff = Vector3Subtract(localEnd, localStart);
        float localDist = Vector3Length(localDiff);
        if (localDist < 0.001f) continue;
        Vector3 localDir = Vector3Scale(localDiff, 1.0f / localDist);
        BoundingBox localBounds = AssetCache::Get().GetOrComputeModelBounds(model);
        if (!SegmentHitsBox(localStart, localDir, localDist, localBounds)) continue;

        for (int i = 0; i < model->meshCount; i++) {
            RayCollision rc = GetRayCollisionMesh(ray, model->meshes[i], world);
            if (rc.hit && rc.distance <= dist && rc.distance < bestDist) {
                bestDist = rc.distance;
                hit = true;
                outActor = actor;
                outLocation = rc.point;
                outNormal = rc.normal;
                outDistance = rc.distance;
            }
        }
    }
    return hit;
}