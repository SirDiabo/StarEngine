#pragma once
#include "Core/Panel.h"
#include "Modules/Stargraph/Stargraph.h"
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
    void RenderGraph();
    void RenderNodeContextMenu();
    void RenderLinkCreation();
    void RenderSidebar();

    void StartLinkFrom(int pinId, bool fromOutput);
    void TryCompleteLink(int pinId, bool onInput);
    void DeleteSelected();

    bool IsInputPin(int pinId) const;
    bool IsOutputPin(int pinId) const;
    bool PinsCompatible(int outputPinId, int inputPinId) const;
    Vector2 GetPinPos(const Node& node, const Pin& pin) const;
    Color GetPinColor(PinType type) const;

    Stargraph graph;
    std::string filePath;
    bool isOpen = false;

    Vector2 scroll{0, 0};
    bool panning = false;
    Vector2 panStart{0, 0};
    Vector2 scrollAtPanStart{0, 0};

    int selectedNodeId = -1;
    bool draggingNode = false;
    Vector2 nodeDragOffset{0, 0};

    int linkFromPin = -1;
    bool linkFromIsOutput = false;

    bool nodeMenuOpen = false;
    bool nodeMenuJustOpened = false;
    Vector2 nodeMenuPos{0, 0};

    bool isDirty = false;

    static const float nodeWidth;
    static const float pinHeight;
    static const float pinRadius;
    static const float headerHeight;
    static const float sidebarWidth;
};
