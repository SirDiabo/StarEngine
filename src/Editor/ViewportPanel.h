#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "raylib.h"
#include <string>
#include <unordered_map>

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ViewportPanel() override;

    void LoadMap(const std::string& path);
    void SaveMap();
    void DropActorAsset(const std::string& assetPath, Vector2 screenPos);
    void DropStaticMeshAsset(const std::string& assetPath, Vector2 screenPos);
    const StarMap* GetStarMap() const { return &starMap; }
    int* GetSelectedActorIndexPtr() { return &selectedActorIndex; }
    const std::string& GetMapPath() const { return mapPath; }
    bool IsMapLoaded() const { return mapLoaded; }

    bool IsPlaying() const { return playing; }

private:
    void EnsureRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();
    void RenderToolbar();
    void HandleMapInteraction(Rectangle client);
    Model* GetOrLoadModel(const std::string& sourceMeshPath);
    void DrawInstanceOrMesh(const std::string& meshAssetPath, Vector3 pos, Vector3 rot, Vector3 scale, Color tint);
    void UnloadMeshCache();

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

    StarMap starMap;
    std::string mapPath;
    bool mapLoaded = false;
    bool mapDirty = false;

    int selectedActorIndex = -1;

    bool playing = false;

    bool flying = false;
    bool orbiting = false;
    Vector2 lookStart{0, 0};
    Vector2 orbitStart{0, 0};
    Vector3 flyPosition{0, 0, 0};

    float yaw = 45.0f;
    float pitch = 35.0f;
    float orbitRadius = 800.0f;
    float flySpeed = 400.0f;

    Vector3 cameraTarget{0, 0, 0};

    float savedYaw = 45.0f;
    float savedPitch = 35.0f;
    float savedOrbitRadius = 800.0f;
    Vector3 savedCameraTarget{0, 0, 0};

    std::unordered_map<std::string, Model> meshCache;

    static const float toolbarHeight;
};