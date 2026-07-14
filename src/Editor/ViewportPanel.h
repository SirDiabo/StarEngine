#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "Modules/Stargraph/Stargraph.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

class Actor;
class LogPanel;

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

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }

    void InvalidateClassGraph(const std::string& graphAssetPath);

private:
    void EnsureRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();
    void RenderToolbar();
    void RenderPlayLogOverlay(Rectangle client);
    void HandleMapInteraction(Rectangle client);
    const Stargraph* GetOrLoadClassGraph(const std::string& graphAssetPath);
    void DrawInstanceOrMesh(const std::string& meshAssetPath, const std::vector<std::string>& materialAssetPaths,
                             Vector3 pos, Vector3 rot, Vector3 scale, bool selected, Color fallbackColor);
    void DrawCollisionShape(const ComponentDecl& comp, Vector3 pos, Vector3 rot, Color color);

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
    LogPanel* logPanel = nullptr;
    int playLogStart = 0;

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

    std::unordered_map<std::string, Stargraph> classGraphCache;

    static const float toolbarHeight;
};