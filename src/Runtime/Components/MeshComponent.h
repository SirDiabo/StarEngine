#pragma once
#include "raylib.h"
#include "SceneComponent.h"
#include <cstdint>
#include <string>
#include <vector>

class MeshComponent : public SceneComponent {
public:
    std::string meshPath;
    std::vector<std::string> materialPaths; // parallel to the mesh asset's material slots
    bool castsShadow = true;

    enum CollisionMode { CollisionMesh, CollisionNone };
    CollisionMode collisionMode = CollisionMesh;
    int collisionLayer = 0;
    uint32_t collisionMask = 0xFFFFFFFFu;

    std::string currentAnimPath;
    bool animPlaying = false;
    bool animLooping = false;
    bool defaultAnimAutoPlay = false;
    float animPlayRate = 1.0f;
    float animTime = 0.0f; // seconds into the current animation

    const char* GetClassName() const override { return "MeshComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
    void SetMeshPath(const std::string& path);
    void SetMaterialPaths(const std::vector<std::string>& paths);
    void SetMaterialPath(int slotIndex, const std::string& path);

    void PlayAnimation(const std::string& animPath, bool loop);
    void StopAnimation();
    void SetAnimPlayRate(float rate);

    Model* GetCachedModel();
    void InvalidateCachedModel();

    ~MeshComponent();

private:
    std::string cachedModelPath;
    Model* cachedModel = nullptr;

    bool hasAnimModelInstance = false;
    std::string animModelInstancePath;
    Model animModelInstance{};
    void EnsureAnimModelInstance();
    void ReleaseAnimModelInstance();

    std::string cachedAnimMetaPath;
    float cachedAnimFps = 30.0f;
    int cachedAnimFrameCount = 0;

    ModelAnimation* GetCurrentAnimation();
};

void RegisterMeshComponentNodes();
