#pragma once
#include "Core/Panel.h"
#include "Modules/Stargraph/Stargraph.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include <string>

class StargraphEditorPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    bool Open(const std::string& filePath);
    void Close();
    bool IsOpen() const { return isOpen; }
    const std::string& GetFilePath() const { return filePath; }

private:
    void RenderToolbar();
    void RenderLeftPanel();
    void RenderGraph();
    void RenderRightPanel();
    void RenderNodeContextMenu();
    void RenderWireInProgress();

    void TryCompleteLink(int pinId, bool onInput);
    void CancelLink();
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

    int selectedNodeId = -1;
    bool draggingNode = false;
    Vector2 nodeDragOffset{0, 0};

    int linkFromPin = -1;
    bool linkFromIsOutput = false;
    bool linkJustStarted = false;

    bool nodeMenuOpen = false;
    bool nodeMenuJustOpened = false;
    Vector2 nodeMenuPos{0, 0};
    int filterCompatibleWithPin = -1;
    bool didPan = false;

    int editingPinId = -1;
    char editBuffer[256] = "";

    static const float nodeWidth;
    static const float pinHeight;
    static const float pinRadius;
    static const float headerHeight;
    static const float leftPanelWidth;
    static const float rightPanelWidth;
    static const float toolbarHeight;
};
