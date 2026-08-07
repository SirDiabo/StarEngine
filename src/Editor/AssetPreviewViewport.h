#pragma once
#include "raylib.h"
#include "Core/StarAsset.h"
#include <string>
#include <vector>

class AssetPreviewViewport {
public:
    void Initialize();
    ~AssetPreviewViewport();

    void SetAsset(const std::string& starAssetPath);
    void OnReimport();
    void SetPreviewAnimation(ModelAnimation* anim, float frame) { previewAnim = anim; previewAnimFrame = frame; }

    void Render(float x, float y, float width, float height);

private:
    void EnsureRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

    std::string assetPath;
    StarAsset asset;

    ModelAnimation* previewAnim = nullptr;
    float previewAnimFrame = 0.0f;

    bool flying = false;
    bool orbiting = false;
    Vector2 lookStart{0, 0};
    Vector2 orbitStart{0, 0};
    Vector3 flyPosition{0, 0, 0};

    float yaw = 45.0f;
    float pitch = 35.0f;
    float orbitRadius = 400.0f;
    float flySpeed = 400.0f;

    Vector3 cameraTarget{0, 0, 0};

    static const float toolbarHeight;
};
