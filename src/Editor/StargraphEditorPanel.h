#pragma once
#include "Core/Panel.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include "AssetPickerPopup.h"
#include <string>
#include <vector>

class ViewportPanel;

class StargraphEditorPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    bool Open(const std::string& filePath);
    void Close();
    bool IsOpen() const { return isOpen; }
    const std::string& GetFilePath() const { return filePath; }
    void SetViewportPanel(ViewportPanel* viewport) { viewportPanel = viewport; }

private:
    void SaveGraph();
    void RenderToolbar();
    void RenderComponentOutliner(Rectangle area);
    void RenderComponentDetails(Rectangle panel, float& y);
    void RenderLeftPanel();
    void RenderGraph();
    void RenderRightPanel();
    void RenderNodeContextMenu();
    void RenderWireInProgress();

    void TryCompleteLink(int pinId, bool onInput);
    void CancelLink();
    void RemoveLinkFromPin(int pinId);
    void DeleteSelected();

    bool PinsCompatible(int outputPinId, int inputPinId) const;
    Vector2 GetPinPos(const Node& node, const Pin& pin) const;
    Color GetPinColor(PinType type) const;
    Color GetNodeHeaderColor(const Node& node) const;
    float GetNodeHeight(const Node& node) const;
    Node* GetNodeAtPos(Vector2 pos);

    Stargraph graph;
    std::string filePath;
    bool isOpen = false;
    bool isDirty = false;

    Vector2 scroll{0, 0};
    bool panning = false;
    Vector2 panStart{0, 0};
    Vector2 scrollAtPanStart{0, 0};
    float zoom = 1.0f;

    int selectedNodeId = -1;
    std::vector<int> selectedNodeIds;
    bool boxSelecting = false;
    Vector2 boxSelectStart{0, 0};
    Vector2 boxSelectEnd{0, 0};
    bool draggingMulti = false;
    Vector2 multiDragAnchor{0, 0};
    std::vector<Vector2> multiDragStartPositions;
    bool draggingNode = false;
    Vector2 nodeDragOffset{0, 0};

    int linkFromPin = -1;
    bool linkFromIsOutput = false;

    bool nodeMenuOpen = false;
    bool nodeMenuJustOpened = false;
    Vector2 nodeMenuPos{0, 0};
    int filterCompatibleWithPin = -1;
    float nodeMenuScrollOffset = 0;
    char nodeMenuSearch[64] = "";
    bool didPan = false;

    int editingPinId = 0;
    char editBuffer[256] = "";

    std::vector<std::string> varValueSnapshot;

    int selectedVariableIndex = -1;
    bool varContextMenuOpen = false;
    int varContextMenuIndex = -1;
    int renamingVariableIndex = -1;
    char varNameBuffer[128] = "";
    bool addingVariable = false;

    static const float nodeWidth;
    static const float pinHeight;
    static const float pinRadius;
    static const float headerHeight;
    static const float leftPanelWidth;
    static const float rightPanelWidth;
    static const float toolbarHeight;

    int variableDragIndex = -1;
    Vector2 variableDragPos{0, 0};
    bool variableDragActive = false;

    int selectedComponentIndex = -1;
    bool addComponentMenuOpen = false;
    Vector2 addComponentMenuPos{0, 0};
    static const float componentOutlinerHeight;

    AssetPickerPopup componentMeshPicker;
    AssetPickerPopup componentMaterialPicker;
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
};