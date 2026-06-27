#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "raylib.h"
#include <string>

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ViewportPanel() override;

    void LoadMap(const std::string& path);
    void SaveMap();
    void DropActorAsset(const std::string& assetPath, Vector2 screenPos);
    const StarMap* GetStarMap() const { return &starMap; }
    int* GetSelectedActorIndexPtr() { return &selectedActorIndex; }

    bool IsPlaying() const { return playing; }

private:
    void EnsureRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();
    void RenderToolbar();
    void HandleMapInteraction(Rectangle client);

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
    float orbitRadius = 20.0f;
    float flySpeed = 10.0f;

    Vector3 cameraTarget{0, 0, 0};

    static const float toolbarHeight;
};