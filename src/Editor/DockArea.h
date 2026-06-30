#pragma once
#include "Core/DockNode.h"
#include "Core/Panel.h"
#include "raylib.h"
#include <vector>
#include <string>
#include <unordered_map>

static const float DOCK_TAB_HEIGHT    = 24.0f;
static const float DOCK_SPLITTER_SIZE = 4.0f;
static const float DOCK_MIN_SIZE      = 60.0f;
static const float DOCK_DROP_ZONE_PCT = 0.25f;

enum class DropZone { None, Left, Right, Top, Bottom, Center };

struct FloatingPanel {
    Panel* panel = nullptr;
    Rectangle bounds = { 200, 200, 400, 300 };
    bool draggingHeader = false;
    Vector2 dragOffset = { 0, 0 };
};

// Manages a panel dock tree and floating panels within one editor tab's area.
class DockArea {
public:
    void SetPanels(const std::vector<Panel*>& panels);
    Panel* GetPanel(const std::string& name) const;

    void SetDefaultLayout(std::unique_ptr<DockNode> root);

    // Main render call - draws everything including overlays.
    void Render(Rectangle area);

    // Draw only the overlays that must appear on top of all other UI (floating panels, drag ghost, drop highlights).
    // Called by the editor after the menu bar body but before the menu popup.
    void RenderOverlays();

    void SaveLayout(std::unordered_map<std::string, std::string>& out, const std::string& prefix) const;
    void LoadLayout(const std::unordered_map<std::string, std::string>& in, const std::string& prefix);

    void SetPanelVisible(const std::string& name, bool visible);
    bool IsPanelVisible(const std::string& name) const;

private:
    void LayoutNode(DockNode* node, Rectangle area);
    // Renders panel content only (no overlays).
    void RenderNode(DockNode* node);
    void RenderSplitter(DockNode* node);
    void RenderTabBar(DockNode* node);

    void EndDrag(Vector2 mousePos);
    void RemoveFromSource();
    void FinishReattach();
    void DetachFromNode(DockNode* node, const std::string& panelName);
    void ReattachToNode(DockNode* node, const std::string& panelName, DropZone zone);
    DockNode* FindLeafAt(Vector2 pos, DockNode* node = nullptr) const;
    DockNode* FindNodeContaining(const std::string& panelName, DockNode* node = nullptr) const;
    DropZone GetDropZone(Rectangle area, Vector2 pos) const;
    void DrawDropHighlight(DockNode* target, DropZone zone) const;
    void PruneEmptyNodes(std::unique_ptr<DockNode>& node);

    // Splitter resize state.
    DockNode* activeSplitter       = nullptr;
    Vector2   splitterDragStart    = { 0, 0 };
    float     splitterRatioAtStart = 0.0f;

    // Pending drag: mouse pressed on a tab, waiting for threshold before committing to drag.
    bool      pendingDrag       = false;
    std::string pendingDragPanel;
    DockNode* pendingDragNode   = nullptr;
    Vector2   pendingDragOrigin = { 0, 0 };

    // Active drag state.
    bool      dragging           = false;
    std::string dragPanelName;
    Vector2   dragPos            = { 0, 0 };
    DockNode* dragSourceNode     = nullptr;
    FloatingPanel* dragFloatingSource = nullptr;

    std::unique_ptr<DockNode> root;
    std::vector<Panel*> panels;
    std::vector<FloatingPanel> floating;

    Rectangle lastArea = { 0, 0, 0, 0 };
};