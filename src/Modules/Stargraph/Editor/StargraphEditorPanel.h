#pragma once
#include "raylib.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/ComponentEditing.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include "GraphCanvas.h"
#include "Editor/AssetPickerPopup.h"
#include "Editor/RegistryPickerPopup.h"
#include "Editor/SearchableListPopup.h"
#include "Editor/ChoicePopup.h"
#include "Core/Widgets/TreeListElement.h"
#include "Editor/GizmoRenderer.h"
#include "Editor/PropertyPanel.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <functional>

class ViewportPanel;
class FunctionUndoDocument;

std::string StargraphPinAssetExtension(const Pin& pin);

template <typename T>
inline void MoveInList(std::vector<T>& items, int fromIndex, int toIndex, TreeDropPosition pos) {
    if (fromIndex < 0 || fromIndex >= (int)items.size()) return;
    if (toIndex < 0 || toIndex >= (int)items.size()) return;
    if (fromIndex == toIndex) return;

    T moved = items[fromIndex];
    items.erase(items.begin() + fromIndex);

    int insertAt = toIndex;
    if (fromIndex < toIndex) insertAt--;
    if (pos == TreeDropPosition::After) insertAt++;
    if (insertAt < 0) insertAt = 0;
    if (insertAt > (int)items.size()) insertAt = (int)items.size();

    items.insert(items.begin() + insertAt, moved);
}

class Widget;
class VerticalBoxElement;
class TextElement;

class StargraphEditorPanel {
public:
    void Initialize();
    ~StargraphEditorPanel();
    void SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive);
    void CaptureUndoState(std::string& content, std::string& selection);
    void RestoreUndoState(const std::string& content, const std::string& selection);

    bool Open(const std::string& filePath);
    bool OpenEmbedded(const Stargraph& initialGraph, const std::string& logicalName);
    void Close();
    bool IsOpen() const { return isOpen; }
    const std::string& GetFilePath() const { return filePath; }
    Stargraph& GetGraph() { return graph; }
    void SetViewportPanel(ViewportPanel* viewport) { viewportPanel = viewport; }
    void SetWidgetElementLister(std::function<std::vector<std::pair<std::string, std::string>>()> lister) { listWidgetElements = std::move(lister); }
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
    std::vector<PropertyRow> BuildDetailsRows();
    std::string DetailsSelectionKey();
    void OnDetailAssetPicked(const std::string& fullRowId, const std::string& pickedPath);
    void RenderDetailPickers(Rectangle area);
    void RenderNodeContextMenu();
    void CloseNodeMenu();
    void SpawnNodeFromMenu(const std::string& nodeTypeIdIn, const std::string& sourceKind);

    void RenderPreviewViewport(Rectangle area);
    void SetPreviewFullscreen(bool v) { previewFullscreen = v; }
    void SetOnTogglePreviewFullscreen(std::function<void()> cb) { onTogglePreviewFullscreen = cb; }

public:
    bool IsDirty() const { return isDirty; }
private:
    void ResetTransientEditState();

    void BuildComponentRows(std::vector<PropertyRow>& rows, int index);
    void BuildVariableRows(std::vector<PropertyRow>& rows, int index, bool isLocal);
    void BuildFunctionRows(std::vector<PropertyRow>& rows, int index);
    void BuildCustomEventRows(std::vector<PropertyRow>& rows, int nodeId);
    void BuildCallCustomEventRows(std::vector<PropertyRow>& rows, int nodeId);
    void BuildCallFunctionRows(std::vector<PropertyRow>& rows, int nodeId);
    void BuildGenericNodeRows(std::vector<PropertyRow>& rows, int nodeId);
    ComponentDecl* DetailComponent(int index);
    void SetComponentField(int index, const std::string& fieldKey, const std::string& value);
    LiveComponentCache liveComponents;
    Variable* DetailVariable(int index, bool isLocal);
    FunctionDef* DetailFunction(int index);
    Node* DetailNode(int nodeId);
    void ResetDetailPickers();
    void RenderWireInProgress();
    void RenderKeyPickerPopup();

    bool ConnectPins(int outputPinId, int inputPinId);
    void TryCompleteLink(int pinId, bool onInput);
    void CancelLink();
    void RemoveLinkFromPin(int pinId);
    void RemoveSpecificLink(int outputPinId, int inputPinId);
    void BeginReroute(int pinId);
    void RefreshWildcardPins(Node* node);
    void ResolveClonedWildcards(const std::vector<Node*>& nodes);
    void DeleteSelected();
    void CopySelectedNodes();
    void PasteNodes();
    void DuplicateSelected();

    bool PinsCompatible(int outputPinId, int inputPinId) const;
    Vector2 GetPinPos(const Node& node, const Pin& pin) const;
    Color GetPinColor(PinType type) const;
    Color GetPinColor(const Pin& pin) const;
    Color GetNodeHeaderColor(const Node& node) const;
    float GetNodeHeight(const Node& node) const;
    float GetNodeWidth(const Node& node) const;
    Node* GetNodeAtPos(Vector2 pos);

    bool IsPrimitiveNode(const Node& node) const;
    float GetPrimitiveNodeHeight() const;

    FunctionUndoDocument* undoDocument = nullptr;
    std::function<void()> undoReveal;
    std::function<bool()> undoOwnerActive;

    Stargraph graph;
    std::string filePath;
    bool isOpen = false;
    bool isDirty = false;
    bool embedded = false;

    int openFunctionIndex = -1;
    Rectangle leftPanelBounds{};

    Widget* leftChrome = nullptr;
    VerticalBoxElement* leftRoot = nullptr;
    TreeListElement* funcList = nullptr;
    TreeListElement* varList = nullptr;
    TreeListElement* localVarList = nullptr;
    VerticalBoxElement* localVarSection = nullptr;
    ChoicePopup leftContextMenu;
    int leftContextKind = -1;
    int leftContextIndex = -1;
    void BuildLeftPanelChrome();
    void BuildComponentChrome();
    Widget* compChrome = nullptr;
    VerticalBoxElement* compRoot = nullptr;
    TreeListElement* compList = nullptr;
    TextElement* compEmptyText = nullptr;
    void ClearOtherLeftSelections(int keepKind);
    void RemoveFunctionAt(int index);
    TextElement* graphNameText = nullptr;
    TextElement* localVarHeader = nullptr;
    int pendingRenameList = -1;
    int pendingRenameIndex = -1;
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
    std::string nodeMenuSearch;
    int nodeMenuHighlighted = 0;
    Widget* nodeMenuChrome = nullptr;
    TextElement* nodeMenuSearchField = nullptr;

    int editingPinId = 0;

    Widget* pinEditChrome = nullptr;
    TextElement* pinEditField = nullptr;
    bool pinEditActiveThisFrame = false;

    std::vector<std::string> varValueSnapshot;

    int selectedVariableIndex = -1;
    int lastValidVariableIndex = -1;
    int listeningForKeyPinId = 0;

    int selectedFunctionIndex = -1;
    int lastValidFunctionIndex = -1;
    int lastFunctionClickIndex = -1;
    float lastFunctionClickTime = -1;

    int selectedLocalVariableIndex = -1;
    int lastValidLocalVariableIndex = -1;

    bool keyPickerOpen = false;
    int keyPickerPinId = 0;
    Vector2 keyPickerPos{0, 0};
    float keyPickerWidth = 0;
    bool keyPickerJustOpened = false;
    SearchableListPopup searchableKeyPicker;
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



    std::string elementDragName;
    std::string elementDragClass;
    Vector2 elementDragPos{0, 0};
    bool elementDragActive = false;

    int selectedComponentIndex = -1;
    int lastValidComponentIndex = -1;
    SearchableListPopup addComponentMenu;

    AssetPickerPopup actorClassPicker;
    int actorClassPickerPinId = 0;
    ViewportPanel* viewportPanel = nullptr;
    std::function<std::vector<std::pair<std::string, std::string>>()> listWidgetElements;
    std::string activeDeviceFieldKey;
    std::string lastDetailsSelectionKey;

    RegistryPickerPopup inputMappingPicker;
    SearchableListPopup inputKeyPicker;
    SearchableListPopup inputDevicePicker;
    int inputKeyPickerMappingIndex = -1;
    int inputKeyPickerBindingIndex = -1;
    int listeningForInputBindingMappingIndex = -1;
    int listeningForInputBindingIndex = -1;

    RegistryPickerPopup savePicker;
    RegistryPickerPopup layerPicker;
    int layerPickerPinId = 0;
    std::string saveTargetFieldKey;
    int saveTargetKind = 0;
    int saveTargetIndex = -1;

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
