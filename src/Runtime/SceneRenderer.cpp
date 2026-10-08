#include "Core/RotationMath.h"
#include "Core/Profiler.h"
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

static void DrawMeshComponent(MeshComponent* mc, Vector3 pos, Vector3 rot, Vector3 scale) {
    Model* model = mc->GetCachedModel();
    if (!model) return;

    Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rotationMatrix = ComposeRotationDegrees(rot);
    Matrix localTransform = MatrixMultiply(scaleMatrix, rotationMatrix);

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

void SceneRenderer::DrawWorldActors(Camera3D camera) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* comp : actor->GetComponents()) {
            if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actor));
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                DrawMeshComponent(mc, pos, rot, scl);
            } else if (BillboardComponent* bc = dynamic_cast<BillboardComponent*>(comp)) {
                PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actor));
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                DrawBillboardComponent(bc, pos, camera);
            }
        }
    }
}
