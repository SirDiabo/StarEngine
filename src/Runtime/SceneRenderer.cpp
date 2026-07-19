#include "SceneRenderer.h"
#include "World.h"
#include "Actor.h"
#include "Components/MeshComponent.h"
#include "AssetCache.h"
#include "Core/StarMaterial.h"
#include "raylib.h"
#include "raymath.h"

static void DrawMeshComponent(MeshComponent* mc, Vector3 pos, Vector3 rot, Vector3 scale) {
    Model* model = mc->GetCachedModel();
    if (!model) return;

    Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
    Matrix localTransform = MatrixMultiply(scaleMatrix, rotationMatrix);

    Matrix savedTransform = model->transform;
    model->transform = localTransform;
    AssetCache::ApplyMaterialSlots(model, mc->materialPaths);
    DrawModel(*model, pos, 1.0f, WHITE);
    model->transform = savedTransform;
}

void SceneRenderer::DrawWorldActors(bool showPlaceholders) {
    for (const auto& actor : World::Get().GetActors()) {
        Vector3 basePos = actor->position;
        Vector3 baseRot = actor->rotation;
        Vector3 baseScale = actor->scale;

        bool drewAnyMesh = false;
        for (Component* comp : actor->GetComponents()) {
            if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                Vector3 pos, rot, scl;
                actor->GetComponentWorldTransform(comp, pos, rot, scl);
                DrawMeshComponent(mc, pos, rot, scl);
                drewAnyMesh = true;
            }
        }

        if (!drewAnyMesh && showPlaceholders) {
            const float placeholderSize = 50.0f;
            Vector3 boxSize = {placeholderSize * baseScale.x, placeholderSize * baseScale.y, placeholderSize * baseScale.z};
            DrawCube(basePos, boxSize.x, boxSize.y, boxSize.z, {100, 200, 100, 255});
            DrawCubeWires(basePos, boxSize.x, boxSize.y, boxSize.z, {0, 0, 0, 120});
        }
    }
}