#include "DockArea.h"
#include "raylib.h"
#include "raygui.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "Core/SafeParse.h"
#include <algorithm>
#include <cstdio>

static const float FLOAT_HEADER_H = 22.0f;
static const float DRAG_THRESHOLD = 6.0f;

// Setup

void DockArea::SetPanels(const std::vector<Panel*>& p) {
    panels = p;
}

Panel* DockArea::GetPanel(const std::string& name) const {
    for (Panel* p : panels)
        if (p->GetName() == name) return p;
    return nullptr;
}

void DockArea::SetDefaultLayout(std::unique_ptr<DockNode> r) {
    root = std::move(r);
    floating.clear();
}

// Layout pass

void DockArea::LayoutNode(DockNode* node, Rectangle area) {
    node->bounds = area;
    if (node->IsLeaf()) return;

    Rectangle a = area, b = area;
    if (node->type == DockNode::Type::SplitH) {
        a.width = (area.width - DOCK_SPLITTER_SIZE) * node->splitRatio;
        b.x     = area.x + a.width + DOCK_SPLITTER_SIZE;
        b.width = area.width - a.width - DOCK_SPLITTER_SIZE;
    } else {
        a.height = (area.height - DOCK_SPLITTER_SIZE) * node->splitRatio;
        b.y      = area.y + a.height + DOCK_SPLITTER_SIZE;
        b.height = area.height - a.height - DOCK_SPLITTER_SIZE;
    }
    LayoutNode(node->first.get(),  a);
    LayoutNode(node->second.get(), b);
}

// Main render: panel content only, no overlays

void DockArea::Render(Rectangle area) {
    lastArea = area;
    if (!root) return;
    // Reset cursor each frame; hover checks below will override if still applicable.
    if (!dragging && !pendingDrag && !activeSplitter)
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    if (!maximizedPanelName.empty()) {
        Panel* p = GetPanel(maximizedPanelName);
        if (p && p->IsVisible()) {
            const UIPalette& theme = UITheme::Get();
            Rectangle bar = { area.x, area.y, area.width, DOCK_TAB_HEIGHT };
            DrawRectangleRec(bar, theme.tabBar);
            DrawText(p->GetName(), (int)bar.x + 10, (int)bar.y + 5, 11, theme.textPrimary);

            Rectangle content = area;
            content.y      += DOCK_TAB_HEIGHT;
            content.height -= DOCK_TAB_HEIGHT;
            p->bounds = content;
            p->Render();
            return;
        }
        maximizedPanelName.clear();
    }

    LayoutNode(root.get(), area);
    RenderNode(root.get());
}

// Overlays: floating panels, drag ghost, drop highlights
// Called by the editor after Render() but before the menu bar popup,
// so these appear above panel content but below the menu.

void DockArea::RenderOverlays() {
    Vector2 mouse = GetMousePosition();

    // Floating panel windows.
    for (FloatingPanel& fp : floating) {
        Panel* p = fp.panel;
        if (!p || !p->IsVisible()) continue;

        const UIPalette& theme = UITheme::Get();

        Rectangle full    = fp.bounds;
        Rectangle header  = { full.x, full.y, full.width, FLOAT_HEADER_H };
        Rectangle content = { full.x, full.y + FLOAT_HEADER_H,
                               full.width, full.height - FLOAT_HEADER_H };

        UIWidget::Panel(full);
        DrawRectangleRec(header, theme.buttonBg);
        DrawText(p->GetName(), (int)header.x + 8,
                 (int)(header.y + (header.height - 11) / 2.0f), 11, theme.tabText);

        Rectangle closeRect = { full.x + full.width - 20, full.y + 3, 16, 16 };
        bool closeHov = CheckCollisionPointRec(mouse, closeRect);
        if (UIWidget::IconButton(closeRect, "x"))
            p->SetVisible(false);

        if (!dragging && !pendingDrag) {
            if (fp.draggingHeader) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    fp.bounds.x = mouse.x - fp.dragOffset.x;
                    fp.bounds.y = mouse.y - fp.dragOffset.y;
                } else {
                    fp.draggingHeader = false;
                    if (CheckCollisionPointRec(mouse, lastArea)) {
                        dragging           = true;
                        dragPanelName      = p->GetName();
                        dragSourceNode     = nullptr;
                        dragFloatingSource = &fp;
                        dragPos            = mouse;
                    }
                }
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                       CheckCollisionPointRec(mouse, header) && !closeHov) {
                fp.draggingHeader = true;
                fp.dragOffset = { mouse.x - full.x, mouse.y - full.y };
            }
        }

        p->bounds = content;
        p->Render();
    }

    // Pending drag: wait for mouse to move past threshold before committing to drag.
    if (pendingDrag) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mouse.x - pendingDragOrigin.x;
            float dy = mouse.y - pendingDragOrigin.y;
            if (dx * dx + dy * dy > DRAG_THRESHOLD * DRAG_THRESHOLD) {
                dragging           = true;
                dragPanelName      = pendingDragPanel;
                dragSourceNode     = pendingDragNode;
                dragFloatingSource = nullptr;
                pendingDrag        = false;
            }
        } else {
            pendingDrag = false;
        }
    }

    // Active drag: drop highlights and ghost.
    if (dragging) {
        dragPos = mouse;

        DockNode* target = FindLeafAt(mouse);
        if (target && target != dragSourceNode) {
            Rectangle tabBar = { target->bounds.x, target->bounds.y,
                                 target->bounds.width, DOCK_TAB_HEIGHT };
            if (CheckCollisionPointRec(mouse, tabBar)) {
                DrawRectangleRec(tabBar, UITheme::Get().dropZone);
                DrawRectangleLinesEx(tabBar, 2, UITheme::Get().dropBorder);
            } else {
                DropZone zone = GetDropZone(target->bounds, mouse);
                if (zone != DropZone::None)
                    DrawDropHighlight(target, zone);
            }
        }

        DrawRectangle((int)mouse.x - 40, (int)mouse.y - 8, 80, 18, { 60, 100, 180, 160 });
        DrawText(dragPanelName.c_str(), (int)mouse.x - 36, (int)mouse.y - 5, 11, WHITE);

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            EndDrag(mouse);
    }
}

// Node render

void DockArea::RenderNode(DockNode* node) {
    if (node->IsLeaf()) {
        RenderTabBar(node);
        if (!node->panelNames.empty()) {
            int idx = node->activeTab;
            if (idx < 0 || idx >= (int)node->panelNames.size()) idx = 0;
            Panel* p = GetPanel(node->panelNames[idx]);
            if (p && p->IsVisible()) {
                Rectangle content = node->bounds;
                content.y      += DOCK_TAB_HEIGHT;
                content.height -= DOCK_TAB_HEIGHT;
                p->bounds = content;
                p->Render();
            }
        }
    } else {
        RenderSplitter(node);
        RenderNode(node->first.get());
        RenderNode(node->second.get());
    }
}

// Tab bar

void DockArea::RenderTabBar(DockNode* node) {
    const UIPalette& theme = UITheme::Get();

    Rectangle bar = { node->bounds.x, node->bounds.y, node->bounds.width, DOCK_TAB_HEIGHT };
    DrawRectangleRec(bar, theme.tabBar);

    Vector2 mouse = GetMousePosition();
    float tabX = bar.x;
    const float tabPad = 10.0f;
    const float closeW = 14.0f;

    for (int i = 0; i < (int)node->panelNames.size(); i++) {
        const std::string& pname = node->panelNames[i];
        float textW = (float)MeasureText(pname.c_str(), 11);
        float tabW  = textW + tabPad * 2 + closeW + 4;

        Rectangle tabRect = { tabX, bar.y, tabW, bar.height };
        bool hovered = CheckCollisionPointRec(mouse, tabRect);
        bool active  = (i == node->activeTab);

        DrawRectangleRec(tabRect, UIWidget::SelectableBg(hovered, active, theme.tabBar));
        DrawRectangle((int)tabRect.x, (int)(tabRect.y + tabRect.height - 1), (int)tabRect.width, 1,
                      active ? theme.accent : Color{ 55, 55, 60, 255 });
        DrawText(pname.c_str(), (int)(tabX + tabPad),
                 (int)(bar.y + (bar.height - 11) / 2.0f), 11, theme.tabText);

        Rectangle closeRect = { tabX + tabW - closeW - 3, bar.y + (bar.height - closeW) / 2.0f, closeW, closeW };
        bool closeHov = CheckCollisionPointRec(mouse, closeRect);
        bool closeClicked = UIWidget::IconButton(closeRect, "x");

        if (closeClicked) {
            SetPanelVisible(pname, false);
            if (node->activeTab >= (int)node->panelNames.size())
                node->activeTab = (int)node->panelNames.size() - 1;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered && !closeHov &&
                   !dragging && !pendingDrag) {
            node->activeTab   = i;
            pendingDrag       = true;
            pendingDragPanel  = pname;
            pendingDragNode   = node;
            pendingDragOrigin = mouse;
        }

        tabX += tabW;
    }
}

// Splitter

void DockArea::RenderSplitter(DockNode* node) {
    bool horiz = (node->type == DockNode::Type::SplitH);
    Rectangle splitter;
    if (horiz) {
        float x = node->first->bounds.x + node->first->bounds.width;
        splitter = { x, node->bounds.y, DOCK_SPLITTER_SIZE, node->bounds.height };
    } else {
        float y = node->first->bounds.y + node->first->bounds.height;
        splitter = { node->bounds.x, y, node->bounds.width, DOCK_SPLITTER_SIZE };
    }

    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, splitter);

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(splitter, (hovered || activeSplitter == node) ? theme.splitterHover : theme.splitter);

    if ((hovered || activeSplitter == node) && !dragging && !pendingDrag)
        SetMouseCursor(horiz ? MOUSE_CURSOR_RESIZE_EW : MOUSE_CURSOR_RESIZE_NS);

    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !dragging && !pendingDrag) {
        activeSplitter       = node;
        splitterDragStart    = mouse;
        splitterRatioAtStart = node->splitRatio;
    }

    if (activeSplitter == node) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float delta, total;
            if (horiz) {
                delta = mouse.x - splitterDragStart.x;
                total = node->bounds.width - DOCK_SPLITTER_SIZE;
            } else {
                delta = mouse.y - splitterDragStart.y;
                total = node->bounds.height - DOCK_SPLITTER_SIZE;
            }
            float newRatio = splitterRatioAtStart + delta / total;
            float minRatio = DOCK_MIN_SIZE / total;
            node->splitRatio = std::max(minRatio, std::min(1.0f - minRatio, newRatio));
        } else {
            activeSplitter = nullptr;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
    }
}

// Drag and drop

void DockArea::EndDrag(Vector2 mousePos) {
    dragging = false;
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    DockNode* target = FindLeafAt(mousePos);

    if (target && target != dragSourceNode) {
        Rectangle tabBar = { target->bounds.x, target->bounds.y,
                             target->bounds.width, DOCK_TAB_HEIGHT };
        // Drop onto another tab bar - merge as a new tab.
        if (CheckCollisionPointRec(mousePos, tabBar)) {
            RemoveFromSource();
            PruneEmptyNodes(root);
            target = FindLeafAt(mousePos);
            if (!target && root && root->IsLeaf()) target = root.get();
            if (target) ReattachToNode(target, dragPanelName, DropZone::Center);
            FinishReattach();
            return;
        }

        // Drop onto a content zone - split.
        DropZone zone = GetDropZone(target->bounds, mousePos);
        if (zone != DropZone::None) {
            RemoveFromSource();
            PruneEmptyNodes(root);
            target = FindLeafAt(mousePos);
            if (!target && root && root->IsLeaf()) target = root.get();
            if (target) ReattachToNode(target, dragPanelName, zone);
            FinishReattach();
            return;
        }
    }

    // Released outside any valid target - become floating.
    if (dragFloatingSource) {
        dragFloatingSource = nullptr;
    } else {
        if (dragSourceNode) {
            DetachFromNode(dragSourceNode, dragPanelName);
            PruneEmptyNodes(root);
        }
        Panel* p = GetPanel(dragPanelName);
        if (p) {
            FloatingPanel fp;
            fp.panel  = p;
            fp.bounds = { mousePos.x - 150, mousePos.y - 10, 400, 300 };
            p->SetVisible(true);
            floating.push_back(fp);
        }
    }

    dragSourceNode     = nullptr;
    dragFloatingSource = nullptr;
}

void DockArea::RemoveFromSource() {
    if (dragFloatingSource) {
        dragFloatingSource->panel->SetVisible(false);
        floating.erase(
            std::remove_if(floating.begin(), floating.end(),
                [this](const FloatingPanel& fp) { return &fp == dragFloatingSource; }),
            floating.end());
        dragFloatingSource = nullptr;
    } else if (dragSourceNode) {
        DetachFromNode(dragSourceNode, dragPanelName);
        dragSourceNode = nullptr;
    }
}

void DockArea::FinishReattach() {
    Panel* p = GetPanel(dragPanelName);
    if (p) p->SetVisible(true);
    dragSourceNode     = nullptr;
    dragFloatingSource = nullptr;
}

void DockArea::DetachFromNode(DockNode* node, const std::string& panelName) {
    auto& names = node->panelNames;
    names.erase(std::remove(names.begin(), names.end(), panelName), names.end());
    if (node->activeTab >= (int)names.size())
        node->activeTab = (int)names.size() - 1;
    if (node->activeTab < 0) node->activeTab = 0;
}

void DockArea::ReattachToNode(DockNode* node, const std::string& panelName, DropZone zone) {
    if (zone == DropZone::Center) {
        node->panelNames.push_back(panelName);
        node->activeTab = (int)node->panelNames.size() - 1;
        return;
    }

    auto newLeaf = std::make_unique<DockNode>();
    newLeaf->panelNames = { panelName };
    newLeaf->activeTab  = 0;
    newLeaf->type       = DockNode::Type::Leaf;

    auto existingLeaf = std::make_unique<DockNode>();
    existingLeaf->panelNames = node->panelNames;
    existingLeaf->activeTab  = node->activeTab;
    existingLeaf->type       = DockNode::Type::Leaf;

    node->panelNames.clear();

    bool newFirst    = (zone == DropZone::Left || zone == DropZone::Top);
    node->type       = (zone == DropZone::Left || zone == DropZone::Right)
                       ? DockNode::Type::SplitH : DockNode::Type::SplitV;
    node->splitRatio = 0.5f;
    node->first      = newFirst ? std::move(newLeaf) : std::move(existingLeaf);
    node->second     = newFirst ? std::move(existingLeaf) : std::move(newLeaf);
}

void DockArea::PruneEmptyNodes(std::unique_ptr<DockNode>& nodePtr) {
    if (!nodePtr) return;
    if (nodePtr->IsLeaf()) return;

    PruneEmptyNodes(nodePtr->first);
    PruneEmptyNodes(nodePtr->second);

    bool firstEmpty  = nodePtr->first  && nodePtr->first->IsLeaf()  && nodePtr->first->panelNames.empty();
    bool secondEmpty = nodePtr->second && nodePtr->second->IsLeaf() && nodePtr->second->panelNames.empty();

    if (firstEmpty  || !nodePtr->first)  { auto t = std::move(nodePtr->second); nodePtr = std::move(t); }
    else if (secondEmpty || !nodePtr->second) { auto t = std::move(nodePtr->first);  nodePtr = std::move(t); }
}

DockNode* DockArea::FindLeafAt(Vector2 pos, DockNode* node) const {
    if (!node) node = root.get();
    if (!node || !CheckCollisionPointRec(pos, node->bounds)) return nullptr;
    if (node->IsLeaf()) return node;
    DockNode* r = FindLeafAt(pos, node->first.get());
    return r ? r : FindLeafAt(pos, node->second.get());
}

DockNode* DockArea::FindNodeContaining(const std::string& panelName, DockNode* node) const {
    if (!node) node = root.get();
    if (!node) return nullptr;
    if (node->IsLeaf()) {
        for (const auto& n : node->panelNames)
            if (n == panelName) return node;
        return nullptr;
    }
    DockNode* r = FindNodeContaining(panelName, node->first.get());
    return r ? r : FindNodeContaining(panelName, node->second.get());
}

DropZone DockArea::GetDropZone(Rectangle area, Vector2 pos) const {
    Rectangle content = { area.x, area.y + DOCK_TAB_HEIGHT,
                          area.width, area.height - DOCK_TAB_HEIGHT };
    if (!CheckCollisionPointRec(pos, content)) return DropZone::None;
    float pct = DOCK_DROP_ZONE_PCT;
    if (pos.x < content.x + content.width  * pct)         return DropZone::Left;
    if (pos.x > content.x + content.width  * (1.0f - pct)) return DropZone::Right;
    if (pos.y < content.y + content.height * pct)         return DropZone::Top;
    if (pos.y > content.y + content.height * (1.0f - pct)) return DropZone::Bottom;
    return DropZone::Center;
}

void DockArea::DrawDropHighlight(DockNode* target, DropZone zone) const {
    Rectangle r = target->bounds;
    Rectangle c = { r.x, r.y + DOCK_TAB_HEIGHT, r.width, r.height - DOCK_TAB_HEIGHT };
    float hw = c.width * 0.5f, hh = c.height * 0.5f;

    Rectangle h = c;
    switch (zone) {
        case DropZone::Left:   h = { c.x,       c.y,      hw, c.height }; break;
        case DropZone::Right:  h = { c.x + hw,  c.y,      hw, c.height }; break;
        case DropZone::Top:    h = { c.x,        c.y,      c.width, hh  }; break;
        case DropZone::Bottom: h = { c.x,        c.y + hh, c.width, hh  }; break;
        case DropZone::Center: h = c; break;
        default: return;
    }
    DrawRectangleRec(h, UITheme::Get().dropZone);
    DrawRectangleLinesEx(h, 2, UITheme::Get().dropBorder);
}

// Visibility

void DockArea::SetPanelVisible(const std::string& name, bool visible) {
    Panel* p = GetPanel(name);
    if (!p) return;

    if (!visible) {
        p->SetVisible(false);
        DockNode* node = FindNodeContaining(name);
        if (node) { DetachFromNode(node, name); PruneEmptyNodes(root); }
        floating.erase(std::remove_if(floating.begin(), floating.end(),
            [&](const FloatingPanel& fp) { return fp.panel == p; }), floating.end());
        return;
    }

    p->SetVisible(true);
    if (FindNodeContaining(name)) return;
    for (auto& fp : floating) if (fp.panel == p) return;

    if (root && root->IsLeaf()) {
        root->panelNames.push_back(name);
        root->activeTab = (int)root->panelNames.size() - 1;
    } else {
        FloatingPanel fp;
        fp.panel  = p;
        fp.bounds = { lastArea.x + 100, lastArea.y + 100, 400, 300 };
        floating.push_back(fp);
    }
}

bool DockArea::IsPanelVisible(const std::string& name) const {
    Panel* p = GetPanel(name);
    return p && p->IsVisible();
}

// Layout serialization

static void SaveNode(const DockNode* node,
                     std::unordered_map<std::string, std::string>& out,
                     const std::string& prefix) {
    if (!node) return;
    out[prefix + ".type"] = std::to_string((int)node->type);
    if (node->IsLeaf()) {
        std::string tabs;
        for (size_t i = 0; i < node->panelNames.size(); i++) {
            if (i) tabs += ",";
            tabs += node->panelNames[i];
        }
        out[prefix + ".tabs"]      = tabs;
        out[prefix + ".activeTab"] = std::to_string(node->activeTab);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", node->splitRatio);
        out[prefix + ".splitRatio"] = buf;
        SaveNode(node->first.get(),  out, prefix + ".0");
        SaveNode(node->second.get(), out, prefix + ".1");
    }
}

static std::unique_ptr<DockNode> LoadNode(
    const std::unordered_map<std::string, std::string>& in,
    const std::string& prefix) {
    auto it = in.find(prefix + ".type");
    if (it == in.end()) return nullptr;

    auto node  = std::make_unique<DockNode>();
    node->type = (DockNode::Type)SafeParse::ToInt(it->second);

    if (node->IsLeaf()) {
        auto itTabs = in.find(prefix + ".tabs");
        if (itTabs != in.end() && !itTabs->second.empty()) {
            std::string s = itTabs->second;
            size_t pos = 0;
            while (pos < s.size()) {
                size_t comma = s.find(',', pos);
                if (comma == std::string::npos) comma = s.size();
                node->panelNames.push_back(s.substr(pos, comma - pos));
                pos = comma + 1;
            }
        }
        auto itTab = in.find(prefix + ".activeTab");
        if (itTab != in.end()) node->activeTab = SafeParse::ToInt(itTab->second);
    } else {
        auto itR = in.find(prefix + ".splitRatio");
        if (itR != in.end()) node->splitRatio = SafeParse::ToFloat(itR->second);
        node->first  = LoadNode(in, prefix + ".0");
        node->second = LoadNode(in, prefix + ".1");
    }
    return node;
}

void DockArea::SaveLayout(std::unordered_map<std::string, std::string>& out,
                           const std::string& prefix) const {
    SaveNode(root.get(), out, prefix);
}

void DockArea::LoadLayout(const std::unordered_map<std::string, std::string>& in,
                           const std::string& prefix) {
    floating.clear();
    auto loaded = LoadNode(in, prefix);
    if (loaded) root = std::move(loaded);
}
