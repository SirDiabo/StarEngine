#include "Core/RotationMath.h"
#include "Core/Culling.h"
#include "Core/MeshInstancing.h"
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
#include <string>
#include <vector>

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

namespace {

constexpr size_t kMinInstancesPerBatch = 4;

struct BatchItem {
    Actor* actor;
    MeshComponent* mesh;
    Vector3 pos;
    Matrix local;
};

struct Batch {
    Model* model = nullptr;
    const std::vector<std::string>* materialPaths = nullptr;
    std::vector<BatchItem> items;
    std::vector<Matrix> worlds;
};

struct PendingBillboard {
    Actor* actor;
    BillboardComponent* billboard;
    Vector3 pos;
};

std::vector<Batch> batchPool;
size_t batchCount = 0;
std::vector<PendingBillboard> pendingBillboards;

Batch& FindOrAddBatch(Model* model, const std::vector<std::string>& materialPaths) {
    for (size_t i = 0; i < batchCount; i++) {
        Batch& batch = batchPool[i];
        if (batch.model == model && *batch.materialPaths == materialPaths) return batch;
    }
    if (batchCount == batchPool.size()) batchPool.emplace_back();
    Batch& batch = batchPool[batchCount++];
    batch.model = model;
    batch.materialPaths = &materialPaths;
    batch.items.clear();
    batch.worlds.clear();
    return batch;
}

void DrawInstancedBatch(Batch& batch) {
    PROFILE_SCOPE("Instanced Meshes");
    PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, std::string("Instanced Meshes"));
    Model* model = batch.model;
    AssetCache::ApplyMaterialSlots(model, *batch.materialPaths);
    for (int i = 0; i < model->meshCount; i++) {
        int materialIndex = model->meshMaterial ? model->meshMaterial[i] : 0;
        if (materialIndex < 0 || materialIndex >= model->materialCount) continue;
        MeshInstancing::Draw(model->meshes[i], model->materials[materialIndex], batch.worlds.data(), (int)batch.worlds.size());
    }
}

}

void SceneRenderer::DrawWorldActors(Camera3D camera) {
    const Culling::Frustum frustum = Culling::FromGLState();
    const bool instancing = MeshInstancing::Supported();
    SceneRenderer::Stats stats;
    batchCount = 0;
    pendingBillboards.clear();

    for (const auto& actor : World::Get().GetActors()) {
        for (Component* comp : actor->GetComponents()) {
            if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                Matrix localTransform = MatrixMultiply(MatrixScale(scl.x, scl.y, scl.z), ComposeRotationDegrees(rot));
                Matrix world = MatrixMultiply(localTransform, MatrixTranslate(pos.x, pos.y, pos.z));

                BoundingBox localBounds;
                if (mc->GetLocalBounds(localBounds)) {
                    BoundingBox worldBounds = Culling::TransformBox(localBounds, world);
                    if (!Culling::Intersects(frustum, worldBounds)) { stats.culledByFrustum++; continue; }
                    float maxDistance = mc->maxDrawDistance > 0.0f ? mc->maxDrawDistance : actor->maxDrawDistance;
                    if (Culling::BeyondDistance(camera.position, worldBounds, maxDistance)) { stats.culledByDistance++; continue; }
                }

                if (instancing && mc->IsInstanceable()) {
                    Batch& batch = FindOrAddBatch(mc->GetCachedModel(), mc->materialPaths);
                    batch.items.push_back({ actor.get(), mc, pos, localTransform });
                    batch.worlds.push_back(world);
                    continue;
                }

                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actor));
                PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, ProfilerLabels::ForActor(*actor));
                mc->EnsurePose();
                DrawMeshComponent(mc, pos, localTransform);
                stats.drawn++;
            } else if (BillboardComponent* bc = dynamic_cast<BillboardComponent*>(comp)) {
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                pendingBillboards.push_back({ actor.get(), bc, pos });
            }
        }
    }

    for (size_t i = 0; i < batchCount; i++) {
        Batch& batch = batchPool[i];
        if (batch.items.size() >= kMinInstancesPerBatch) {
            DrawInstancedBatch(batch);
            stats.instanced += (int)batch.items.size();
        } else {
            for (const BatchItem& item : batch.items) {
                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*item.actor));
                PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, ProfilerLabels::ForActor(*item.actor));
                DrawMeshComponent(item.mesh, item.pos, item.local);
            }
        }
        stats.drawn += (int)batch.items.size();
    }

    // Billboards blend, so they go after the opaque meshes.
    for (const PendingBillboard& pending : pendingBillboards) {
        PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*pending.actor));
        PROFILE_GPU_SCOPE_DYN_L(Profiler::DetailVerbose, ProfilerLabels::ForActor(*pending.actor));
        DrawBillboardComponent(pending.billboard, pending.pos, camera);
    }
    lastStats = stats;
}
