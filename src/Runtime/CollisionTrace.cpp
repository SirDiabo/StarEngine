#include "CollisionTrace.h"
#include "Actor.h"
#include "World.h"
#include "Components/MeshComponent.h"
#include "raymath.h"

static Matrix ComponentWorldMatrix(Actor* actor, Component* comp) {
    Vector3 pos, rot, scl;
    actor->GetComponentWorldTransform(comp, pos, rot, scl);
    Matrix scaleM = MatrixScale(scl.x, scl.y, scl.z);
    Matrix rotM = MatrixRotateXYZ({ DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z });
    Matrix transM = MatrixTranslate(pos.x, pos.y, pos.z);
    return MatrixMultiply(MatrixMultiply(scaleM, rotM), transM);
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
