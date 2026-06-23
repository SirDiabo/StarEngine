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

    bool IsPlaying() const { return playing; }

private:
    void EnsureRenderTarget(int width, int height);
    void UpdateCamera(Rectangle graphArea);
    void RenderScene();
    void RenderToolbar();
    void RenderActorContextMenu();
    void HandleMapInteraction(Rectangle graphArea);

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

    StarMap starMap;
    std::string mapPath;
    bool mapLoaded = false;
    bool mapDirty = false;

    int selectedActorIndex = -1;
    bool actorMenuOpen = false;
    bool actorMenuJustOpened = false;
    Vector2 actorMenuPos{0, 0};
    Vector2 actorMenuWorldPos{0, 0};

    bool playing = false;

    bool orbiting = false;
    Vector2 orbitStart{0, 0};
    float yaw = 45.0f;
    float pitch = 35.0f;
    float orbitRadius = 20.0f;

    static const float toolbarHeight;
};