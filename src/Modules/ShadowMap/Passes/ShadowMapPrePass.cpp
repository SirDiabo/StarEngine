#include "ShadowMapPrePass.h"
#include "Core/ShadingModel.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/LightComponent.h"
#include "rlgl.h"
#include "raymath.h"

namespace {

const int kShadowMapSize = 2048;
const float kSunOrthoHalfSize = 900.0f;
const float kSunPullback = 1200.0f;
const float kPointLightFovy = 100.0f;
const float kShadowStrength = 0.75f;

RenderTexture2D shadowTarget = {0};
bool shadowTargetLoaded = false;

Shader depthShader = {0};
Material depthMaterial = {0};
bool depthMaterialLoaded = false;

RenderTexture2D LoadDepthOnlyRenderTexture(int size) {
    RenderTexture2D target = {0};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) return target;

    rlEnableFramebuffer(target.id);

    target.texture.width = size;
    target.texture.height = size;

    target.depth.id = rlLoadTextureDepth(size, size, false);
    target.depth.width = size;
    target.depth.height = size;

    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferComplete(target.id);

    rlDisableFramebuffer();
    return target;
}

void EnsureResourcesLoaded() {
    if (!shadowTargetLoaded) {
        shadowTarget = LoadDepthOnlyRenderTexture(kShadowMapSize);
        shadowTargetLoaded = true;
    }
    if (!depthMaterialLoaded) {
        depthShader = LoadShaderFromMemory(
            "#version 330\nin vec3 vertexPosition;\nuniform mat4 mvp;\nvoid main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }\n",
            "#version 330\nvoid main() { }\n");
        depthMaterial = LoadMaterialDefault();
        depthMaterial.shader = depthShader;
        depthMaterialLoaded = true;
    }
}

struct ShadowCasterLight {
    bool isSun = false;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 direction{0.0f, -1.0f, 0.0f};
};

bool FindShadowCasterLight(Vector3 aroundPos, ShadowCasterLight& outCaster) {
    bool haveSun = false;
    ShadowCasterLight sun;

    bool haveNearest = false;
    ShadowCasterLight nearest;
    float bestDistSq = 1e30f;

    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            LightComponent* lc = dynamic_cast<LightComponent*>(comp);
            if (!lc || lc->lightType == LightComponent::Global) continue;

            if (lc->lightType == LightComponent::Sun) {
                if (!haveSun) {
                    Vector3 rot = actor->GetComponentWorldRotation(lc);
                    float yr = rot.y * DEG2RAD;
                    float pr = rot.x * DEG2RAD;
                    sun.isSun = true;
                    sun.direction = Vector3Normalize({cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)});
                    haveSun = true;
                }
                continue;
            }

            Vector3 pos = actor->GetComponentWorldPosition(lc);
            float distSq = Vector3DistanceSqr(pos, aroundPos);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                nearest.isSun = false;
                nearest.position = pos;
                haveNearest = true;
            }
        }
    }

    if (haveSun) { outCaster = sun; return true; }
    if (haveNearest) { outCaster = nearest; return true; }
    return false;
}

void BuildLightCamera(Camera3D camera, const ShadowCasterLight& caster, Camera3D& outLightCamera, Vector3& outLightDirToSurface) {
    if (caster.isSun) {
        Vector3 lightDir = caster.direction;
        Vector3 up0 = fabsf(lightDir.y) > 0.99f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};
        Vector3 rightAxis = Vector3Normalize(Vector3CrossProduct(lightDir, up0));
        Vector3 upAxis = Vector3CrossProduct(rightAxis, lightDir);

        float texelWorldSize = (kSunOrthoHalfSize * 2.0f) / (float)kShadowMapSize;
        float rightDist = Vector3DotProduct(camera.position, rightAxis);
        float upDist = Vector3DotProduct(camera.position, upAxis);
        float snappedRight = floorf(rightDist / texelWorldSize) * texelWorldSize;
        float snappedUp = floorf(upDist / texelWorldSize) * texelWorldSize;
        float forwardDist = Vector3DotProduct(camera.position, lightDir);

        Vector3 focus = Vector3Add(
            Vector3Add(Vector3Scale(rightAxis, snappedRight), Vector3Scale(upAxis, snappedUp)),
            Vector3Scale(lightDir, forwardDist));

        outLightCamera.position = Vector3Subtract(focus, Vector3Scale(lightDir, kSunPullback));
        outLightCamera.target = focus;
        outLightCamera.up = upAxis;
        outLightCamera.fovy = kSunOrthoHalfSize * 2.0f;
        outLightCamera.projection = CAMERA_ORTHOGRAPHIC;

        outLightDirToSurface = Vector3Negate(lightDir);
        return;
    }

    Vector3 toFocus = Vector3Subtract(camera.position, caster.position);
    float dist = Vector3Length(toFocus);
    Vector3 forward = dist > 0.001f ? Vector3Scale(toFocus, 1.0f / dist) : Vector3{0.0f, -1.0f, 0.0f};
    Vector3 up = fabsf(forward.y) > 0.99f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};

    outLightCamera.position = caster.position;
    outLightCamera.target = camera.position;
    outLightCamera.up = up;
    outLightCamera.fovy = kPointLightFovy;
    outLightCamera.projection = CAMERA_PERSPECTIVE;

    outLightDirToSurface = Vector3Negate(forward);
}

void DrawShadowCasters() {
    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor) continue;
        for (Component* comp : actor->GetComponents()) {
            MeshComponent* mc = dynamic_cast<MeshComponent*>(comp);
            if (!mc || !mc->castsShadow) continue;

            Model* model = mc->GetCachedModel();
            if (!model) continue;

            Vector3 pos, rot, scl;
            actor->GetComponentWorldTransform(comp, pos, rot, scl);

            Matrix scaleMatrix = MatrixScale(scl.x, scl.y, scl.z);
            Matrix rotationMatrix = MatrixRotateXYZ({DEG2RAD * rot.x, DEG2RAD * rot.y, DEG2RAD * rot.z});
            Matrix localTransform = MatrixMultiply(scaleMatrix, rotationMatrix);
            Matrix worldTransform = MatrixMultiply(localTransform, MatrixTranslate(pos.x, pos.y, pos.z));

            for (int i = 0; i < model->meshCount; i++) {
                DrawMesh(model->meshes[i], depthMaterial, worldTransform);
            }
        }
    }
}

} // namespace

namespace ShadowMapPrePass {

void Run(Camera3D camera) {
    EnsureResourcesLoaded();
    if (!shadowTargetLoaded || shadowTarget.id == 0) return;

    ShadowCasterLight caster;
    if (!FindShadowCasterLight(camera.position, caster)) return;

    Camera3D lightCamera{};
    Vector3 lightDirToSurface;
    BuildLightCamera(camera, caster, lightCamera, lightDirToSurface);

    BeginTextureMode(shadowTarget);
    ClearBackground(BLANK);
    BeginMode3D(lightCamera);

    Matrix lightView = rlGetMatrixModelview();
    Matrix lightProj = rlGetMatrixProjection();

    DrawShadowCasters();

    EndMode3D();
    EndTextureMode();

    Matrix lightViewProj = MatrixMultiply(lightView, lightProj);
    ShadingModel::SetShadowSource(shadowTarget.depth.id, lightViewProj, lightDirToSurface, kShadowMapSize, kShadowStrength);
}

void Shutdown() {
    if (depthMaterialLoaded) {
        UnloadMaterial(depthMaterial);
        depthMaterialLoaded = false;
    }
    if (shadowTargetLoaded && shadowTarget.id != 0) {
        rlUnloadFramebuffer(shadowTarget.id);
    }
    shadowTarget = RenderTexture2D{0};
    shadowTargetLoaded = false;
    ShadingModel::ResetShadowSource();
}

}
