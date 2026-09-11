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

class DockArea {
public:
    void SetPanels(const std::vector<Panel*>& panels);
    Panel* GetPanel(const std::string& name) const;

    void SetDefaultLayout(std::unique_ptr<DockNode> root);
    std::unique_ptr<DockNode> TakeLayout();
    const DockNode* GetLayoutRoot() const { return root.get(); }

    static void SaveLayoutTree(const DockNode* node, std::unordered_map<std::string, std::string>& out, const std::string& prefix);
    static std::unique_ptr<DockNode> LoadLayoutTree(const std::unordered_map<std::string, std::string>& in, const std::string& prefix);

    void Render(Rectangle area);

    void RenderOverlays();

    void SaveLayout(std::unordered_map<std::string, std::string>& out, const std::string& prefix) const;
    void LoadLayout(const std::unordered_map<std::string, std::string>& in, const std::string& prefix);

    void SetPanelVisible(const std::string& name, bool visible);
    bool IsPanelVisible(const std::string& name) const;

    void SetMaximizedPanel(const std::string& name) { maximizedPanelName = name; }
    void ToggleMaximizedPanel(const std::string& name) {
        maximizedPanelName = (maximizedPanelName == name) ? "" : name;
    }
    bool IsPanelMaximized(const std::string& name) const { return maximizedPanelName == name; }
    bool HasMaximizedPanel() const { return !maximizedPanelName.empty(); }

private:
    void LayoutNode(DockNode* node, Rectangle area);
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

    DockNode* activeSplitter       = nullptr;
    Vector2   splitterDragStart    = { 0, 0 };
    float     splitterRatioAtStart = 0.0f;

    bool      pendingDrag       = false;
    std::string pendingDragPanel;
    DockNode* pendingDragNode   = nullptr;
    Vector2   pendingDragOrigin = { 0, 0 };

    bool      dragging           = false;
    std::string dragPanelName;
    Vector2   dragPos            = { 0, 0 };
    DockNode* dragSourceNode     = nullptr;
    FloatingPanel* dragFloatingSource = nullptr;

    std::unique_ptr<DockNode> root;
    std::vector<Panel*> panels;
    std::vector<FloatingPanel> floating;

    std::string maximizedPanelName;

    Rectangle lastArea = { 0, 0, 0, 0 };
};
