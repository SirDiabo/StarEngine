#pragma once
#include "raylib.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include "GraphCanvas.h"
#include "Editor/AssetPickerPopup.h"
#include "Editor/RegistryPickerPopup.h"
#include "Editor/SearchableListPopup.h"
#include <string>
#include <vector>

class ViewportPanel;

class StargraphEditorPanel {
public:
    void Initialize();

    bool Open(const std::string& filePath);
    void Close();
    bool IsOpen() const { return isOpen; }
    const std::string& GetFilePath() const { return filePath; }
    void SetViewportPanel(ViewportPanel* viewport) { viewportPanel = viewport; }

    void SetScreenBounds(Rectangle b) { screenBounds = b; }

    static float GetToolbarHeight() { return toolbarHeight; }

    void SaveGraph();
    void RenderToolbar(Rectangle area);
    void RenderComponentOutliner(Rectangle area);
    void RenderLeftPanel(Rectangle area);
    void RenderGraph(Rectangle area);
    void RenderRightPanel(Rectangle area);
    void RenderNodeContextMenu();

private:
    void RenderComponentDetails(Rectangle panel, float& y);
    void RenderCustomEventDetails(Rectangle panel, float y, Node& node);
    void RenderCallCustomEventDetails(Rectangle panel, float y, Node& node);
    void RenderVariableArrayEditor(Rectangle panel, float& y, Variable& var, int& activeField);
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
    bool varContextMenuOpen = false;
    int varContextMenuIndex = -1;
    int renamingVariableIndex = -1;
    char varNameBuffer[128] = "";
    bool addingVariable = false;
    int variableNameActiveField = 0;
    int listeningForKeyPinId = 0;

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

    int selectedComponentIndex = -1;
    int lastValidComponentIndex = -1;
    SearchableListPopup addComponentMenu;

    AssetPickerPopup componentMeshPicker;
    AssetPickerPopup componentMaterialPicker;
    AssetPickerPopup componentAnimPicker;
    int materialSlotPickerIndex = -1;
    AssetPickerPopup audioPathPicker;
    AssetPickerPopup actorClassPicker;
    int actorClassPickerPinId = 0;
    ViewportPanel* viewportPanel = nullptr;
    int lastComponentDetailIndex = -1;
    int componentActiveField = 0;
    char compNameBuf[128] = "";
    char compPosBuf[3][32] = {"", "", ""};
    char compRotBuf[3][32] = {"", "", ""};
    char compScaleBuf[3][32] = {"", "", ""};
    char compRadiusBuf[32] = "";
    char compHeightBuf[32] = "";
    char compSoundBuf[256] = "";
    char compVolumeBuf[32] = "";
    char compPitchBuf[32] = "";
    char compFovBuf[32] = "";
    char compNearClipBuf[32] = "";
    char compFarClipBuf[32] = "";
    char compLightColorBuf[3][32] = {"", "", ""};
    char compLightIntensityBuf[32] = "";
    char compLightRadiusBuf[32] = "";
    char compLightInnerConeBuf[32] = "";
    char compLightOuterConeBuf[32] = "";
    char compLightWidthBuf[32] = "";
    char compLightHeightBuf[32] = "";

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
    char eventParamNameBuf[8][64] = {};

    RegistryPickerPopup savePicker;
    std::string saveTargetFieldKey;

    bool showSelfActorDetails = false;
};