#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "Runtime/ActorAsset.h"
#include "BasicActorDefs.h"
#include "GizmoRenderer.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class Actor;
class LogPanel;
class Widget;
class TextElement;

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ViewportPanel() override;

    void LoadMap(const std::string& path);
    void SaveMap();
    void MarkDirty() { mapDirty = true; }
    void DropActorAsset(const std::string& assetPath, Vector2 screenPos);
    void DropStaticMeshAsset(const std::string& assetPath, Vector2 screenPos);
    void DropBasicActor(const BasicActorDef& def, Vector2 screenPos);
    const StarMap* GetStarMap() const { return &starMap; }
    int* GetSelectedActorIndexPtr() { return &selectedActorIndex; }
    const std::string& GetMapPath() const { return mapPath; }
    bool IsMapLoaded() const { return mapLoaded; }
    bool IsMapDirty() const { return mapDirty; }
    bool IsDirty() const override { return mapDirty; }

    bool IsPlaying() const { return playing; }

    void CaptureUndoState(std::string& content, std::string& selection) const;
    void RestoreUndoState(const std::string& content, const std::string& selection);
    bool CanTrackUndo() const { return mapLoaded && !playing; }
    std::function<void()> onMapReplaced;

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }
    void InvalidateClass(const std::string& classAssetPath);
    void SetPackageCallback(std::function<void()> callback) { onPackageRequested = callback; }

    bool isFullscreen = false;
    std::function<void()> onToggleFullscreen;

private:
    void EnsureRenderTarget(int width, int height);
    void EnsurePieRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();
    void RenderToolbar();
    void EnsureToolbarChrome();
    void ResetFpsStats();
    void UpdateFpsStats(float dt);
    void HandleMapInteraction(Rectangle client);
    const ActorAssetData* GetOrLoadClass(const std::string& classAssetPath);
    void DrawInstanceOrMesh(const std::string& meshAssetPath, const std::vector<std::string>& materialAssetPaths,
                             Vector3 pos, Vector3 rot, Vector3 scale, bool selected, Color fallbackColor);
    void DrawActorBillboard(const std::string& iconFile, Vector3 pos, Color tint);
    Texture2D GetOrLoadEditorIcon(const std::string& iconFile);
    bool ToolbarIconToggle(Rectangle r, const std::string& iconFile, bool active);

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

    RenderTexture2D pieRenderTarget{};
    int pieRenderTargetWidth = 0;
    int pieRenderTargetHeight = 0;

    StarMap starMap;
    StarMap playMap;
    std::string mapPath;
    bool mapLoaded = false;
    bool mapDirty = false;

    int selectedActorIndex = -1;

    bool playing = false;
    LogPanel* logPanel = nullptr;
    std::function<void()> onPackageRequested;

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

    std::unordered_map<std::string, Texture2D> editorIcons;

    GizmoMode gizmoMode = GizmoMode::Select;
    GizmoSpace gizmoSpace = GizmoSpace::World;
    GizmoDragState gizmoDragState;

    bool gridSnapEnabled = false;
    float gridStepSize = 100.0f;
    char gridStepBuf[32] = "100.0";

    Widget* toolbarChrome = nullptr;
    TextElement* gridStepField = nullptr;
    TextElement* fpsValueField = nullptr;
    TextElement* fpsStatsField = nullptr;

    float fpsSampleTime = 0.0f;
    int fpsSampleFrames = 0;
    float fpsWindowFps = 0.0f;
    float fpsWindowAvgMs = 0.0f;
    float fpsSessionTime = 0.0f;
    int fpsSessionFrames = 0;
    float fpsSessionAvgFps = 0.0f;
    float fpsBestFrameMs = 0.0f;
    float fpsWorstFrameMs = 0.0f;
    bool fpsSampleValid = false;
    bool fpsStatsDirty = true;

    static const float toolbarHeight;
};
