#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"
#include "Modules/Stargraph/Stargraph.h"
#include "BasicActorDefs.h"
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class Actor;
class LogPanel;

enum class GizmoMode { Select, Move, Rotate, Scale };
enum class GizmoSpace { World, Local };

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

    bool IsPlaying() const { return playing; }

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }
    void InvalidateClassGraph(const std::string& graphAssetPath);
    void SetPackageCallback(std::function<void()> callback) { onPackageRequested = callback; }

private:
    void EnsureRenderTarget(int width, int height);
    void EnsurePieRenderTarget(int width, int height);
    void UpdateCamera(Rectangle client);
    void RenderScene();
    void RenderToolbar();
    void HandleMapInteraction(Rectangle client);
    const Stargraph* GetOrLoadClassGraph(const std::string& graphAssetPath);
    void DrawInstanceOrMesh(const std::string& meshAssetPath, const std::vector<std::string>& materialAssetPaths,
                             Vector3 pos, Vector3 rot, Vector3 scale, bool selected, Color fallbackColor);
    void DrawCollisionShape(const ComponentDecl& comp, Vector3 pos, Vector3 rot, Color color);
    void DrawActorBillboard(const std::string& iconFile, Vector3 pos, Color tint);
    Texture2D GetOrLoadEditorIcon(const std::string& iconFile);

    Vector2 WorldToScreenPoint(Vector3 worldPos) const;
    float GetWorldPerPixel(Vector3 pos) const;
    float GetGizmoWorldScale(Vector3 pos) const;
    void GetGizmoAxes(Vector3 rot, Vector3& axisX, Vector3& axisY, Vector3& axisZ) const;
    void RenderGizmo(Vector3 pos, Vector3 rot);
    int PickGizmoHandle(Vector2 localMp, Vector3 pos, Vector3 rot) const;
    bool UpdateGizmoDrag(Rectangle client);
    bool GizmoToggleButton(Rectangle r, const std::string& iconFile, bool active);

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;

    RenderTexture2D pieRenderTarget{};
    int pieRenderTargetWidth = 0;
    int pieRenderTargetHeight = 0;

    StarMap starMap;
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
    int gizmoHoverAxis = -1;
    int gizmoDragAxis = -1;
    Vector2 gizmoDragStartMouse{0, 0};
    Vector3 gizmoDragStartPos{0, 0, 0};
    Vector3 gizmoDragStartRot{0, 0, 0};
    Vector3 gizmoDragStartScale{0, 0, 0};
    Vector2 gizmoDragAxisDir2D{1, 0};
    float gizmoDragWorldPerPixel = 1.0f;
    float gizmoDragHandleLen = 1.0f;
    float gizmoDragStartScreenAngle = 0.0f;
    float gizmoDragRotSign = 1.0f;

    bool gridSnapEnabled = false;
    float gridStepSize = 100.0f;
    char gridStepBuf[32] = "100.0";
    int toolbarActiveField = 0;

    static const float toolbarHeight;
};