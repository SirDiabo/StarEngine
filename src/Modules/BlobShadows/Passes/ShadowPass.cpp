#include "ShadowPass.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/LightComponent.h"
#include "Runtime/CollisionTrace.h"
#include "rlgl.h"
#include "raymath.h"
#include <vector>

namespace {

const float kMaxCastDistance = 2000.0f;
const float kCastStartLift = 2.0f;
const float kDecalLift = 0.5f;
const float kBaseRadius = 40.0f;
const float kMaxOffset = kBaseRadius * 2.0f;
const float kMaxStretch = 2.5f;

Texture2D shadowTexture = {0};
bool shadowTextureLoaded = false;

void EnsureShadowTextureLoaded() {
    if (shadowTextureLoaded) return;
    Image img = GenImageGradientRadial(64, 64, 0.2f, {0, 0, 0, 160}, {0, 0, 0, 0});
    shadowTexture = LoadTextureFromImage(img);
    UnloadImage(img);
    shadowTextureLoaded = true;
}

struct ShadowLight {
    bool isSun;
    Vector3 position;
    Vector3 direction;
};

void GatherShadowLights(std::vector<ShadowLight>& outLights) {
    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            LightComponent* lc = dynamic_cast<LightComponent*>(comp);
            if (!lc || lc->lightType == LightComponent::Global) continue;

            if (lc->lightType == LightComponent::Sun) {
                Vector3 rot = actor->GetComponentWorldRotation(lc);
                float yr = rot.y * DEG2RAD;
                float pr = rot.x * DEG2RAD;
                Vector3 dir = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
                outLights.push_back({true, {0, 0, 0}, dir});
                continue;
            }

            outLights.push_back({false, actor->GetComponentWorldPosition(lc), {0, 0, 0}});
        }
    }
}

bool FindShadowDirection(const std::vector<ShadowLight>& lights, Vector3 fromPos, Vector3& outDirection) {
    const ShadowLight* nearest = nullptr;
    const ShadowLight* sun = nullptr;
    float bestDistSq = 1e30f;
    Vector3 nearestDir{0, -1, 0};

    for (const auto& light : lights) {
        if (light.isSun) {
            if (!sun) sun = &light;
            continue;
        }
        float distSq = Vector3DistanceSqr(light.position, fromPos);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            nearest = &light;
            nearestDir = Vector3Normalize(Vector3Subtract(fromPos, light.position));
        }
    }

    if (nearest) { outDirection = nearestDir; return true; }
    if (sun) { outDirection = sun->direction; return true; }
    return false;
}

void DrawShadowDecal(Vector3 hitPoint, Vector3 hitNormal, Vector3 lightDir, bool haveLight) {
    Vector3 tangent;
    float grazing = 0.0f;

    if (haveLight) {
        Vector3 onPlane = Vector3Subtract(lightDir, Vector3Scale(hitNormal, Vector3DotProduct(lightDir, hitNormal)));
        float onPlaneLen = Vector3Length(onPlane);
        tangent = onPlaneLen > 0.001f ? Vector3Scale(onPlane, 1.0f / onPlaneLen) : Vector3{1, 0, 0};
        grazing = Clamp(1.0f - fabsf(lightDir.y), 0.0f, 0.85f);
    } else {
        tangent = {1, 0, 0};
    }

    Vector3 bitangent = Vector3CrossProduct(hitNormal, tangent);
    Vector3 center = Vector3Add(hitPoint, Vector3Scale(hitNormal, kDecalLift));
    center = Vector3Add(center, Vector3Scale(tangent, grazing * kMaxOffset));

    float radiusTangent = kBaseRadius * (1.0f + grazing * (kMaxStretch - 1.0f));
    float radiusBitangent = kBaseRadius;

    Vector3 tOff = Vector3Scale(tangent, radiusTangent);
    Vector3 bOff = Vector3Scale(bitangent, radiusBitangent);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(center, tOff), bOff);
    Vector3 p1 = Vector3Add(Vector3Subtract(center, tOff), bOff);
    Vector3 p2 = Vector3Add(Vector3Add(center, tOff), bOff);
    Vector3 p3 = Vector3Subtract(Vector3Add(center, tOff), bOff);

    rlSetTexture(shadowTexture.id);
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlNormal3f(hitNormal.x, hitNormal.y, hitNormal.z);
        rlTexCoord2f(0, 0); rlVertex3f(p0.x, p0.y, p0.z);
        rlTexCoord2f(0, 1); rlVertex3f(p1.x, p1.y, p1.z);
        rlTexCoord2f(1, 1); rlVertex3f(p2.x, p2.y, p2.z);
        rlTexCoord2f(1, 0); rlVertex3f(p3.x, p3.y, p3.z);
    rlEnd();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlSetTexture(0);
}

} // namespace

namespace ShadowPass {

void Run() {
    EnsureShadowTextureLoaded();

    std::vector<ShadowLight> lights;
    GatherShadowLights(lights);

    CollisionTrace::ScopedBatch collisionBatch;

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            MeshComponent* mc = dynamic_cast<MeshComponent*>(comp);
            if (!mc || !mc->castsShadow) continue;

            Vector3 casterPos = actor->GetComponentWorldPosition(mc);
            Vector3 start = Vector3Add(casterPos, {0, kCastStartLift, 0});
            Vector3 end = Vector3Add(casterPos, {0, -kMaxCastDistance, 0});

            Actor* hitActor = nullptr;
            Vector3 hitPoint, hitNormal;
            float hitDist = 0.0f;
            if (!CollisionTrace::TraceAgainstMeshes(start, end, actor, hitActor, hitPoint, hitNormal, hitDist)) continue;

            Vector3 lightDir;
            bool haveLight = FindShadowDirection(lights, casterPos, lightDir);
            DrawShadowDecal(hitPoint, hitNormal, lightDir, haveLight);
        }
    }
}

}
