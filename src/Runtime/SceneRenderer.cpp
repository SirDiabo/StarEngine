#include "Core/RotationMath.h"
#include "Core/Culling.h"
#include "Core/ProfilerRender.h"
#include "ProfilerLabels.h"
#include "SceneRenderer.h"
#include "World.h"
#include "Actor.h"
#include "Components/MeshComponent.h"
#include "Components/BillboardComponent.h"
#include "AssetCache.h"
#include "Core/StarMaterial.h"
#include "raylib.h"
#include "raymath.h"

static void DrawMeshComponent(MeshComponent* mc, Vector3 pos, const Matrix& localTransform) {
    Model* model = mc->GetCachedModel();
    if (!model) return;

    Matrix savedTransform = model->transform;
    model->transform = localTransform;
    AssetCache::ApplyMaterialSlots(model, mc->materialPaths);
    DrawModel(*model, pos, 1.0f, WHITE);
    model->transform = savedTransform;
}

static void DrawBillboardComponent(BillboardComponent* bc, Vector3 pos, Camera3D camera) {
    if (bc->hiddenInGame || bc->texturePath.empty()) return;

    Texture2D* tex = AssetCache::Get().GetOrLoadTexture(bc->texturePath);
    if (!tex) return;

    DrawBillboardRec(camera, *tex, {0, 0, (float)tex->width, (float)tex->height},
                      pos, {bc->width, bc->height}, WHITE);
}

static SceneRenderer::Stats lastStats;

const SceneRenderer::Stats& SceneRenderer::GetLastStats() {
    return lastStats;
}

void SceneRenderer::DrawWorldActors(Camera3D camera) {
    const Culling::Frustum frustum = Culling::FromGLState();
    SceneRenderer::Stats stats;
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* comp : actor->GetComponents()) {
            if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                Matrix localTransform = MatrixMultiply(MatrixScale(scl.x, scl.y, scl.z), ComposeRotationDegrees(rot));

                BoundingBox localBounds;
                if (mc->GetLocalBounds(localBounds)) {
                    Matrix world = MatrixMultiply(localTransform, MatrixTranslate(pos.x, pos.y, pos.z));
                    BoundingBox worldBounds = Culling::TransformBox(localBounds, world);
                    if (!Culling::Intersects(frustum, worldBounds)) { stats.culledByFrustum++; continue; }
                    float maxDistance = mc->maxDrawDistance > 0.0f ? mc->maxDrawDistance : actor->maxDrawDistance;
                    if (Culling::BeyondDistance(camera.position, worldBounds, maxDistance)) { stats.culledByDistance++; continue; }
                }

                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actor));
                PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, ProfilerLabels::ForActor(*actor));
                mc->EnsurePose();
                DrawMeshComponent(mc, pos, localTransform);
                stats.drawn++;
            } else if (BillboardComponent* bc = dynamic_cast<BillboardComponent*>(comp)) {
                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actor));
                PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, ProfilerLabels::ForActor(*actor));
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                DrawBillboardComponent(bc, pos, camera);
            }
        }
    }
    lastStats = stats;
}
