#include "ShadowMapPrePass.h"
#include "Core/ShadingModel.h"
#include "Core/RenderSettings.h"
#include "Modules/ShadowMap/ShadowMapSettings.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/MeshComponent.h"
#include "Runtime/Components/LightComponent.h"
#include "rlgl.h"
#include "raymath.h"

namespace {

const float kSunNear = 1.0f;
const float kSunPullbackRatio = 1.5f;
const int kMinCubeFaceSize = 256;

RenderTexture2D shadowTarget = {0};
bool shadowTargetLoaded = false;
int shadowTargetSize = 0;

RenderTexture2D cubeFaceTargets[6];
bool cubeFacesLoaded = false;
int cubeFaceSize = 0;

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
    int desiredSize = ShadowMapSettings::Get().GetShadowMapSize();
    if (shadowTarget.id == 0 || shadowTargetSize != desiredSize) {
        if (shadowTarget.id != 0) rlUnloadFramebuffer(shadowTarget.id);
        shadowTarget = LoadDepthOnlyRenderTexture(desiredSize);
        shadowTargetSize = desiredSize;
        shadowTargetLoaded = shadowTarget.id != 0;
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

void EnsureCubeFacesLoaded() {
    int desiredFaceSize = ShadowMapSettings::Get().GetShadowMapSize() / 2;
    if (desiredFaceSize < kMinCubeFaceSize) desiredFaceSize = kMinCubeFaceSize;

    if (cubeFacesLoaded && cubeFaceSize == desiredFaceSize) return;

    if (cubeFacesLoaded) {
        for (int i = 0; i < 6; i++) {
            if (cubeFaceTargets[i].id != 0) rlUnloadFramebuffer(cubeFaceTargets[i].id);
        }
    }
    for (int i = 0; i < 6; i++) {
        cubeFaceTargets[i] = LoadDepthOnlyRenderTexture(desiredFaceSize);
    }
    cubeFaceSize = desiredFaceSize;
    cubeFacesLoaded = true;
}

struct ShadowCasterLight {
    LightComponent::LightType type = LightComponent::Sun;
    Actor* ownerActor = nullptr;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 direction{0.0f, -1.0f, 0.0f};
    float outerConeAngle = 35.0f;
    float radius = 500.0f;
};

Vector3 DirectionFromRotation(Vector3 rot) {
    float yr = rot.y * DEG2RAD;
    float pr = rot.x * DEG2RAD;
    return Vector3Normalize({cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)});
}

bool FindShadowCasterLight(Vector3 aroundPos, ShadowCasterLight& outCaster) {
    bool haveSun = false;
    ShadowCasterLight sun;
    sun.type = LightComponent::Sun;

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
                    sun.ownerActor = actor;
                    sun.direction = DirectionFromRotation(actor->GetComponentWorldRotation(lc));
                    haveSun = true;
                }
                continue;
            }

            Vector3 pos = actor->GetComponentWorldPosition(lc);
            float distSq = Vector3DistanceSqr(pos, aroundPos);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                nearest.type = lc->lightType;
                nearest.ownerActor = actor;
                nearest.position = pos;
                nearest.outerConeAngle = lc->outerConeAngle;
                nearest.radius = lc->radius;
                if (lc->lightType == LightComponent::Spot) {
                    nearest.direction = DirectionFromRotation(actor->GetComponentWorldRotation(lc));
                }
                haveNearest = true;
            }
        }
    }

    if (haveSun) { outCaster = sun; return true; }
    if (haveNearest) { outCaster = nearest; return true; }
    return false;
}

void BuildSunCamera(Camera3D camera, const ShadowCasterLight& caster, float halfSize, float pullback,
                     Camera3D& outLightCamera, Vector3& outLightDirToSurface) {
    Vector3 lightDir = caster.direction;
    Vector3 up0 = fabsf(lightDir.y) > 0.99f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};
    Vector3 rightAxis = Vector3Normalize(Vector3CrossProduct(lightDir, up0));
    Vector3 upAxis = Vector3CrossProduct(rightAxis, lightDir);

    float texelWorldSize = (halfSize * 2.0f) / (float)shadowTargetSize;
    float rightDist = Vector3DotProduct(camera.position, rightAxis);
    float upDist = Vector3DotProduct(camera.position, upAxis);
    float snappedRight = floorf(rightDist / texelWorldSize) * texelWorldSize;
    float snappedUp = floorf(upDist / texelWorldSize) * texelWorldSize;
    float forwardDist = Vector3DotProduct(camera.position, lightDir);

    Vector3 focus = Vector3Add(
        Vector3Add(Vector3Scale(rightAxis, snappedRight), Vector3Scale(upAxis, snappedUp)),
        Vector3Scale(lightDir, forwardDist));

    outLightCamera.position = Vector3Subtract(focus, Vector3Scale(lightDir, pullback));
    outLightCamera.target = focus;
    outLightCamera.up = upAxis;
    outLightCamera.fovy = halfSize * 2.0f;
    outLightCamera.projection = CAMERA_ORTHOGRAPHIC;

    outLightDirToSurface = Vector3Negate(lightDir);
}

void BuildSpotCamera(const ShadowCasterLight& caster, Camera3D& outLightCamera) {
    Vector3 dir = caster.direction;
    Vector3 up = fabsf(dir.y) > 0.99f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};

    outLightCamera.position = caster.position;
    outLightCamera.target = Vector3Add(caster.position, dir);
    outLightCamera.up = up;
    outLightCamera.fovy = Clamp(caster.outerConeAngle * 2.0f + 5.0f, 20.0f, 170.0f);
    outLightCamera.projection = CAMERA_PERSPECTIVE;
}

void BuildCubeFaceCamera(Vector3 lightPos, int face, Camera3D& outLightCamera) {
    static const Vector3 dirs[6] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
    };
    static const Vector3 ups[6] = {
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    };

    outLightCamera.position = lightPos;
    outLightCamera.target = Vector3Add(lightPos, dirs[face]);
    outLightCamera.up = ups[face];
    outLightCamera.fovy = 90.0f;
    outLightCamera.projection = CAMERA_PERSPECTIVE;
}

void DrawShadowCasters(Actor* excludeActor) {
    rlEnableBackfaceCulling();
    for (const auto& actorPtr : World::Get().GetActors()) {
        Actor* actor = actorPtr.get();
        if (!actor || actor == excludeActor) continue;
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

void RunSinglePass(Camera3D lightCamera, Vector3 lightDirToSurface, Actor* excludeActor, float shadowStrength, int aaMethod) {
    BeginTextureMode(shadowTarget);
    ClearBackground(BLANK);
    BeginMode3D(lightCamera);

    Matrix lightView = rlGetMatrixModelview();
    Matrix lightProj = rlGetMatrixProjection();

    DrawShadowCasters(excludeActor);

    EndMode3D();
    EndTextureMode();

    Matrix lightViewProj = MatrixMultiply(lightView, lightProj);
    ShadingModel::SetShadowSource(shadowTarget.depth.id, lightViewProj, lightDirToSurface, shadowTargetSize, shadowStrength, aaMethod);
}

void RunCubePass(Vector3 lightPos, Actor* excludeActor, float shadowStrength, int aaMethod) {
    EnsureCubeFacesLoaded();

    unsigned int faceTextureIds[6];
    Matrix faceViewProj[6];

    for (int face = 0; face < 6; face++) {
        Camera3D faceCamera{};
        BuildCubeFaceCamera(lightPos, face, faceCamera);

        BeginTextureMode(cubeFaceTargets[face]);
        ClearBackground(BLANK);
        BeginMode3D(faceCamera);

        Matrix faceView = rlGetMatrixModelview();
        Matrix faceProj = rlGetMatrixProjection();

        DrawShadowCasters(excludeActor);

        EndMode3D();
        EndTextureMode();

        faceTextureIds[face] = cubeFaceTargets[face].depth.id;
        faceViewProj[face] = MatrixMultiply(faceView, faceProj);
    }

    ShadingModel::SetShadowSourceCube(lightPos, faceTextureIds, faceViewProj, cubeFaceSize, shadowStrength, aaMethod);
}

} // namespace

namespace ShadowMapPrePass {

void Run(Camera3D camera) {
    EnsureResourcesLoaded();
    if (!shadowTargetLoaded || shadowTarget.id == 0) return;

    if (RenderSettings::Get().GetLightingQuality() != LightingQuality::PerPixel) {
        RenderSettings::Get().SetLightingQuality(LightingQuality::PerPixel);
    }

    ShadowCasterLight caster;
    if (!FindShadowCasterLight(camera.position, caster)) return;

    double savedNear = rlGetCullDistanceNear();
    double savedFar = rlGetCullDistanceFar();
    float shadowStrength = ShadowMapSettings::Get().GetShadowStrength();
    int aaMethod = (int)ShadowMapSettings::Get().GetShadowAAMethod();

    if (caster.type == LightComponent::Sun) {
        float halfSize = ShadowMapSettings::Get().GetSunCoverageHalfSize();
        float pullback = halfSize * kSunPullbackRatio;
        rlSetClipPlanes(kSunNear, pullback * 2.5);
        Camera3D lightCamera{};
        Vector3 lightDirToSurface;
        BuildSunCamera(camera, caster, halfSize, pullback, lightCamera, lightDirToSurface);
        RunSinglePass(lightCamera, lightDirToSurface, caster.ownerActor, shadowStrength, aaMethod);
    } else if (caster.type == LightComponent::Spot) {
        float near = fmaxf(caster.radius * 0.01f, 2.0f);
        rlSetClipPlanes(near, caster.radius);
        Camera3D lightCamera{};
        BuildSpotCamera(caster, lightCamera);
        RunSinglePass(lightCamera, Vector3Negate(caster.direction), caster.ownerActor, shadowStrength, aaMethod);
    } else {
        float near = fmaxf(caster.radius * 0.01f, 2.0f);
        rlSetClipPlanes(near, caster.radius);
        RunCubePass(caster.position, caster.ownerActor, shadowStrength, aaMethod);
    }

    rlSetClipPlanes(savedNear, savedFar);
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
    shadowTargetSize = 0;

    if (cubeFacesLoaded) {
        for (int i = 0; i < 6; i++) {
            if (cubeFaceTargets[i].id != 0) rlUnloadFramebuffer(cubeFaceTargets[i].id);
            cubeFaceTargets[i] = RenderTexture2D{0};
        }
        cubeFacesLoaded = false;
        cubeFaceSize = 0;
    }

    ShadingModel::ResetShadowSource();
}

}
