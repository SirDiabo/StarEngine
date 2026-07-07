#include "SceneRenderer.h"
#include "World.h"
#include "Actor.h"
#include "Components/MeshComponent.h"
#include "AssetCache.h"
#include "Core/StarAsset.h"
#include "Core/StarMaterial.h"
#include "raylib.h"
#include "raymath.h"

static void DrawMeshComponent(MeshComponent* mc, Vector3 pos, Vector3 rot, Vector3 scale) {
    Model* model = nullptr;
    if (!mc->meshPath.empty()) {
        StarAsset asset;
        if (StarAsset::LoadFromFile(mc->meshPath, asset) && asset.assetType == "StaticMesh") {
            model = AssetCache::Get().GetOrLoadModel(asset.sourcePath);
        }
    }
    if (!model) return;

    Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
    model->transform = MatrixMultiply(scaleMatrix, rotationMatrix);

    Color tint = WHITE;
    if (!mc->materialPath.empty()) {
        StarMaterial material;
        if (StarMaterial::LoadFromFile(mc->materialPath, material)) {
            tint = material.baseColor;
            if (!material.textureAssetPath.empty()) {
                if (Texture2D* tex = AssetCache::Get().GetOrLoadTexture(material.textureAssetPath)) {
                    for (int i = 0; i < model->materialCount; i++) {
                        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
                    }
                }
            }
        }
    }

    DrawModel(*model, pos, 1.0f, tint);
}

void SceneRenderer::DrawWorldActors(bool showPlaceholders) {
    for (const auto& actor : World::Get().GetActors()) {
        Vector3 basePos = actor->position;
        Vector3 baseRot = actor->rotation;
        Vector3 baseScale = actor->scale;

        bool drewAnyMesh = false;
        for (Component* comp : actor->GetComponents()) {
            if (MeshComponent* mc = dynamic_cast<MeshComponent*>(comp)) {
                Vector3 pos = Actor::ComputeWorldPosition(comp->position, actor->position, actor->rotation, actor->scale);
                Vector3 rot = Actor::ComputeWorldRotation(comp->rotation, actor->rotation);
                Vector3 scl = Actor::ComputeWorldScale(comp->scale, actor->scale);
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
