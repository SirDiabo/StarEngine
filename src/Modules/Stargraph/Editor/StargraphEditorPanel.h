#pragma once
#include "raylib.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include "GraphCanvas.h"
#include "Editor/AssetPickerPopup.h"
#include "Editor/RegistryPickerPopup.h"
#include "Editor/SearchableListPopup.h"
#include "Editor/GizmoRenderer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <functional>

class ViewportPanel;

class StargraphEditorPanel {
public:
    void Initialize();
    ~StargraphEditorPanel();

    bool Open(const std::string& filePath);
    bool OpenEmbedded(const Stargraph& initialGraph, const std::string& logicalName);
    void Close();
    bool IsOpen() const { return isOpen; }
    const std::string& GetFilePath() const { return filePath; }
    Stargraph& GetGraph() { return graph; }
    void SetViewportPanel(ViewportPanel* viewport) { viewportPanel = viewport; }
    void BeginElementDrag(const std::string& elementName, const std::string& elementClass) {
        elementDragName = elementName;
        elementDragClass = elementClass;
        elementDragPos = GetMousePosition();
        elementDragActive = true;
    }

    void SetScreenBounds(Rectangle b) { screenBounds = b; }

    static float GetToolbarHeight() { return toolbarHeight; }

    void SaveGraph();
    void RenderToolbar(Rectangle area);
    void RenderComponentOutliner(Rectangle area);
    void RenderLeftPanel(Rectangle area);
    void RenderLeftPanelPopups();
    void RenderGraph(Rectangle area);
    void RenderRightPanel(Rectangle area);
    void RenderNodeContextMenu();

    void RenderPreviewViewport(Rectangle area);
    void SetPreviewFullscreen(bool v) { previewFullscreen = v; }
    void SetOnTogglePreviewFullscreen(std::function<void()> cb) { onTogglePreviewFullscreen = cb; }

private:
    void ResetTransientEditState();

    void RenderComponentDetails(Rectangle panel, float& y);
    void RenderCustomEventDetails(Rectangle panel, float y, Node& node);
    void RenderCallCustomEventDetails(Rectangle panel, float y, Node& node);
    void RenderCallFunctionDetails(Rectangle panel, float y, Node& node);
    void RenderVariableArrayEditor(Rectangle panel, float& y, Variable& var, int& activeField);
    void RenderVariableDetailsBody(Rectangle panel, float y, Variable& var, int detailIndex, bool isLocal);
    void RenderFunctionDetails(Rectangle panel, float y, FunctionDef& fn, int detailIndex);
    void RenderWireInProgress();
    void RenderKeyPickerPopup();

    void TryCompleteLink(int pinId, bool onInput);
    void CancelLink();
    void RemoveLinkFromPin(int pinId);
    void RemoveSpecificLink(int outputPinId, int inputPinId);
    void BeginReroute(int pinId);
    void RefreshWildcardPins(Node* node);
    void DeleteSelected();
    void CopySelectedNodes();
    void PasteNodes();
    void DuplicateSelected();

    bool PinsCompatible(int outputPinId, int inputPinId) const;
    Vector2 GetPinPos(const Node& node, const Pin& pin) const;
    Color GetPinColor(PinType type) const;
    Color GetNodeHeaderColor(const Node& node) const;
    float GetNodeHeight(const Node& node) const;
    float GetNodeWidth(const Node& node) const;
    Node* GetNodeAtPos(Vector2 pos);

    bool IsPrimitiveNode(const Node& node) const;
    float GetPrimitiveNodeHeight() const;

    Stargraph graph;
    std::string filePath;
    bool isOpen = false;
    bool isDirty = false;
    bool embedded = false;

    int openFunctionIndex = -1;
    Rectangle leftPanelBounds{};
    Stargraph& ActiveGraph() {
        if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size())
            return graph.GetFunctions()[openFunctionIndex].graph;
        return graph;
    }
    const Stargraph& ActiveGraph() const {
        if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size())
            return graph.GetFunctions()[openFunctionIndex].graph;
        return graph;
    }
    void OpenFunctionGraph(int index);
    void CloseFunctionGraph();

    GraphCanvas::PanZoomState canvasState;

    int selectedNodeId = -1;
    std::vector<int> selectedNodeIds;
    bool boxSelecting = false;
    Vector2 boxSelectStart{0, 0};
    Vector2 boxSelectEnd{0, 0};
    std::vector<Vector2> multiDragStartPositions;
    bool draggingNode = false;
    Vector2 nodeDragOffset{0, 0};

    int linkFromPin = -1;
    bool linkFromIsOutput = false;
    std::vector<int> rerouteExtraPins;

    bool nodeMenuOpen = false;
    bool nodeMenuJustOpened = false;
    Vector2 nodeMenuPos{0, 0};
    int filterCompatibleWithPin = -1;
    float nodeMenuScrollOffset = 0;
    char nodeMenuSearch[64] = "";

    int editingPinId = 0;
    char editBuffer[256] = "";

    std::vector<std::string> varValueSnapshot;

    int selectedVariableIndex = -1;
    int lastValidVariableIndex = -1;
    bool varContextMenuOpen = false;
    int varContextMenuIndex = -1;
    float varContextMenuAnchorY = 0;
    int renamingVariableIndex = -1;
    char varNameBuffer[128] = "";
    bool addingVariable = false;
    int variableNameActiveField = 0;
    bool variableDragIsLocal = false;
    int listeningForKeyPinId = 0;

    int selectedFunctionIndex = -1;
    int lastValidFunctionIndex = -1;
    bool funcContextMenuOpen = false;
    int funcContextMenuIndex = -1;
    float funcContextMenuAnchorY = 0;
    int renamingFunctionIndex = -1;
    char funcNameBuffer[128] = "";
    bool addingFunction = false;
    int functionNameActiveField = 0;
    int lastFunctionClickIndex = -1;
    float lastFunctionClickTime = -1;

    int selectedLocalVariableIndex = -1;
    int lastValidLocalVariableIndex = -1;
    bool localVarContextMenuOpen = false;
    int localVarContextMenuIndex = -1;
    float localVarContextMenuAnchorY = 0;
    int renamingLocalVariableIndex = -1;
    char localVarNameBuffer[128] = "";
    bool addingLocalVariable = false;
    int localVariableNameActiveField = 0;

    struct ArrayElemBuf { char text[3][64] = {"", "", ""}; };
    std::vector<ArrayElemBuf> varArrayBufs;

    bool keyPickerOpen = false;
    int keyPickerPinId = 0;
    Vector2 keyPickerPos{0, 0};
    float keyPickerWidth = 0;
    char keyPickerSearch[64] = "";
    float keyPickerScroll = 0;
    bool keyPickerJustOpened = false;
    enum class PinPickerKind { Key, FunctionName, ComponentRef, MappingName };
    PinPickerKind keyPickerKind = PinPickerKind::Key;

    static const float minNodeWidth;
    static const float pinHeight;
    static const float pinRadius;
    static const float headerHeight;
    static const float toolbarHeight;
    static const float primitiveNodeHeight;

    Rectangle graphArea{};
    Rectangle screenBounds{};

    int variableDragIndex = -1;
    Vector2 variableDragPos{0, 0};
    bool variableDragActive = false;

    int componentDragIndex = -1;
    Vector2 componentDragPos{0, 0};
    bool componentDragActive = false;

    std::string elementDragName;
    std::string elementDragClass;
    Vector2 elementDragPos{0, 0};
    bool elementDragActive = false;

    int selectedComponentIndex = -1;
    int lastValidComponentIndex = -1;
    SearchableListPopup addComponentMenu;

    AssetPickerPopup componentMaterialPicker;
    AssetPickerPopup audioPathPicker;
    int materialSlotPickerIndex = -1;
    AssetPickerPopup actorClassPicker;
    int actorClassPickerPinId = 0;
    int actorClassPickerVarIndex = -1;
    ViewportPanel* viewportPanel = nullptr;
    int lastComponentDetailIndex = -1;
    int componentActiveField = 0;
    char compNameBuf[128] = "";
    char compPosBuf[3][32] = {"", "", ""};
    char compRotBuf[3][32] = {"", "", ""};
    char compScaleBuf[3][32] = {"", "", ""};
    std::unordered_map<std::string, std::array<char, 32>> genericFieldBufs;
    std::unordered_map<std::string, AssetPickerPopup> genericAssetPickers;
    std::string activeDeviceFieldKey;

    RegistryPickerPopup inputMappingPicker;
    SearchableListPopup inputKeyPicker;
    SearchableListPopup inputDevicePicker;
    int inputKeyPickerMappingIndex = -1;
    int inputKeyPickerBindingIndex = -1;
    int listeningForInputBindingMappingIndex = -1;
    int listeningForInputBindingIndex = -1;
    char compInputScaleBuf[8][32] = {};

    int lastEventDetailNodeId = -1;
    char eventNameBuf[128] = "";
    int eventActiveField = 0;

    int lastFuncCallDetailNodeId = -1;
    char funcCallNameBuf[128] = "";
    int funcCallActiveField = 0;
    char eventParamNameBuf[8][64] = {};

    int lastFuncDetailIndex = -1;
    char funcNameEditBuf[128] = "";
    int funcDetailActiveField = 0;
    char funcInputNameBuf[8][64] = {};
    char funcOutputNameBuf[8][64] = {};

    RegistryPickerPopup savePicker;
    std::string saveTargetFieldKey;

    bool showSelfActorDetails = false;

    void UpdatePreviewCamera(Rectangle client);
    void RenderPreviewScene();
    void HandlePreviewInteraction(Rectangle client);
    void RenderPreviewToolbar(Rectangle toolbarArea);
    void EnsurePreviewRenderTarget(int width, int height);
    Texture2D GetOrLoadPreviewIcon(const std::string& iconFile);

    Camera3D previewCamera{{0, 0, 0}, {0, 0, 0}, {0, 1, 0}, 50.0f, CAMERA_PERSPECTIVE};
    RenderTexture2D previewRenderTarget{};
    int previewRenderTargetWidth = 0;
    int previewRenderTargetHeight = 0;

    bool previewFlying = false;
    bool previewOrbiting = false;
    Vector2 previewLookStart{0, 0};
    Vector2 previewOrbitStart{0, 0};
    Vector3 previewFlyPosition{0, 0, 0};
    float previewYaw = 45.0f;
    float previewPitch = 35.0f;
    float previewOrbitRadius = 400.0f;
    float previewFlySpeed = 400.0f;
    Vector3 previewCameraTarget{0, 0, 0};

    std::unordered_map<std::string, Texture2D> previewIcons;

    GizmoMode previewGizmoMode = GizmoMode::Select;
    GizmoSpace previewGizmoSpace = GizmoSpace::World;
    GizmoDragState previewGizmoState;

    bool previewFullscreen = false;
    std::function<void()> onTogglePreviewFullscreen;

    static const float previewToolbarHeight;
};
