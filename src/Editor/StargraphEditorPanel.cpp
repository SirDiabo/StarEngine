#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "raygui.h"
#include "raymath.h"
#include <algorithm>

const float StargraphEditorPanel::nodeWidth = 180.0f;
const float StargraphEditorPanel::pinHeight = 20.0f;
const float StargraphEditorPanel::pinRadius = 5.0f;
const float StargraphEditorPanel::headerHeight = 24.0f;
const float StargraphEditorPanel::sidebarWidth = 200.0f;

void StargraphEditorPanel::Initialize() {
    name = "Stargraph";
}

bool StargraphEditorPanel::Open(const std::string& path) {
    if (!graph.LoadFromFile(path)) return false;
    filePath = path;
    isOpen = true;
    isDirty = false;
    selectedNodeId = -1;
    linkFromPin = -1;
    nodeMenuOpen = false;
    scroll = {0, 0};
    return true;
}

void StargraphEditorPanel::Close() {
    isOpen = false;
    filePath.clear();
    isDirty = false;
}

Color StargraphEditorPanel::GetPinColor(PinType type) const {
    switch (type) {
        case PinType::Exec:    return {220, 220, 220, 255};
        case PinType::Boolean: return {160,  60,  60, 255};
        case PinType::Integer: return { 90, 130, 200, 255};
        case PinType::Float:   return {130, 200, 130, 255};
        case PinType::String:  return {200, 140,  70, 255};
        case PinType::Vec3:    return {160,  90, 200, 255};
        case PinType::Object:  return { 60, 180, 180, 255};
        default:               return {150, 150, 150, 255};
    }
}

Vector2 StargraphEditorPanel::GetPinPos(const Node& node, const Pin& pin) const {
    Rectangle graphArea = {bounds.x, bounds.y + 25, bounds.width - sidebarWidth, bounds.height - 25};

    float nodeX = graphArea.x + node.posX + scroll.x;
    float nodeY = graphArea.y + node.posY + scroll.y;

    int inputIndex = 0;
    int outputIndex = 0;
    for (const Pin& p : node.inputs) {
        if (p.id == pin.id) {
            float y = nodeY + headerHeight + inputIndex * pinHeight + pinHeight * 0.5f;
            return {nodeX + pinRadius + 2, y};
        }
        inputIndex++;
    }
    for (const Pin& p : node.outputs) {
        if (p.id == pin.id) {
            float y = nodeY + headerHeight + outputIndex * pinHeight + pinHeight * 0.5f;
            return {nodeX + nodeWidth - pinRadius - 2, y};
        }
        outputIndex++;
    }
    return {0, 0};
}

bool StargraphEditorPanel::IsInputPin(int pinId) const {
    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) return true;
        }
    }
    return false;
}

bool StargraphEditorPanel::IsOutputPin(int pinId) const {
    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) return true;
        }
    }
    return false;
}

bool StargraphEditorPanel::PinsCompatible(int outputPinId, int inputPinId) const {
    const Pin* out = nullptr;
    const Pin* in = nullptr;
    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) {
            if (pin.id == outputPinId) out = &pin;
        }
        for (const Pin& pin : node.inputs) {
            if (pin.id == inputPinId) in = &pin;
        }
    }
    if (!out || !in) return false;
    return out->type == in->type;
}

void StargraphEditorPanel::StartLinkFrom(int pinId, bool fromOutput) {
    linkFromPin = pinId;
    linkFromIsOutput = fromOutput;
}

void StargraphEditorPanel::TryCompleteLink(int pinId, bool onInput) {
    if (linkFromPin == -1) return;
    if (linkFromIsOutput == onInput) { linkFromPin = -1; return; }

    int outputPinId = linkFromIsOutput ? linkFromPin : pinId;
    int inputPinId  = linkFromIsOutput ? pinId : linkFromPin;

    if (!PinsCompatible(outputPinId, inputPinId)) { linkFromPin = -1; return; }

    for (const Link& existing : graph.GetLinks()) {
        if (existing.inputPinId == inputPinId) { linkFromPin = -1; return; }
    }

    graph.AddLink({outputPinId, inputPinId});
    isDirty = true;
    linkFromPin = -1;
}

void StargraphEditorPanel::DeleteSelected() {
    if (selectedNodeId == -1) return;
    std::vector<int> pinIds;
    for (const Node& node : graph.GetNodes()) {
        if (std::stoi(node.id) == selectedNodeId) {
            for (const Pin& p : node.inputs)  pinIds.push_back(p.id);
            for (const Pin& p : node.outputs) pinIds.push_back(p.id);
        }
    }
    Stargraph rebuilt;
    rebuilt.assetName = graph.assetName;
    rebuilt.parentClass = graph.parentClass;
    rebuilt.formatVersion = graph.formatVersion;
    for (const Variable& v : graph.GetVariables()) rebuilt.AddVariable(v);
    for (const Node& node : graph.GetNodes()) {
        if (std::stoi(node.id) == selectedNodeId) continue;
        Node* newNode = rebuilt.CreateNode(node.type);
        newNode->posX = node.posX;
        newNode->posY = node.posY;
        for (size_t i = 0; i < node.inputs.size() && i < newNode->inputs.size(); i++) {
            newNode->inputs[i].defaultValue = node.inputs[i].defaultValue;
        }
    }
    graph = std::move(rebuilt);
    selectedNodeId = -1;
    isDirty = true;
}

void StargraphEditorPanel::RenderLinkCreation() {
    if (linkFromPin == -1) return;

    Vector2 mousePos = GetMousePosition();
    Vector2 fromPos = {0, 0};

    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) {
            if (pin.id == linkFromPin) fromPos = GetPinPos(node, pin);
        }
        for (const Pin& pin : node.inputs) {
            if (pin.id == linkFromPin) fromPos = GetPinPos(node, pin);
        }
    }

    DrawLineBezier(fromPos, mousePos, 2.0f, {180, 180, 180, 200});

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) linkFromPin = -1;
}

void StargraphEditorPanel::RenderGraph() {
    Rectangle graphArea = {bounds.x, bounds.y + 25, bounds.width - sidebarWidth, bounds.height - 25};
    Vector2 mousePos = GetMousePosition();

    DrawRectangleRec(graphArea, {28, 28, 28, 255});

    // grid
    int gridStep = 24;
    for (float x = fmod(scroll.x, gridStep) + graphArea.x; x < graphArea.x + graphArea.width; x += gridStep) {
        DrawLineV({x, graphArea.y}, {x, graphArea.y + graphArea.height}, {38, 38, 38, 255});
    }
    for (float y = fmod(scroll.y, gridStep) + graphArea.y; y < graphArea.y + graphArea.height; y += gridStep) {
        DrawLineV({graphArea.x, y}, {graphArea.x + graphArea.width, y}, {38, 38, 38, 255});
    }

    BeginScissorMode((int)graphArea.x, (int)graphArea.y, (int)graphArea.width, (int)graphArea.height);

    // draw links
    for (const Link& link : graph.GetLinks()) {
        const Pin* outPin = nullptr;
        const Pin* inPin = nullptr;
        const Node* outNode = nullptr;
        const Node* inNode = nullptr;
        for (const Node& node : graph.GetNodes()) {
            for (const Pin& pin : node.outputs) {
                if (pin.id == link.outputPinId) { outPin = &pin; outNode = &node; }
            }
            for (const Pin& pin : node.inputs) {
                if (pin.id == link.inputPinId) { inPin = &pin; inNode = &node; }
            }
        }
        if (!outPin || !inPin || !outNode || !inNode) continue;

        Vector2 from = GetPinPos(*outNode, *outPin);
        Vector2 to   = GetPinPos(*inNode, *inPin);
        Color linkColor = GetPinColor(outPin->type);
        DrawLineBezier(from, to, 2.0f, linkColor);
    }

    RenderLinkCreation();

    // draw nodes
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        int nodeIdInt = std::stoi(node.id);
        float nodeX = graphArea.x + node.posX + scroll.x;
        float nodeY = graphArea.y + node.posY + scroll.y;
        float nodeH = headerHeight + std::max(node.inputs.size(), node.outputs.size()) * pinHeight + 4;

        Rectangle nodeRect = {nodeX, nodeY, nodeWidth, nodeH};
        bool isSelected = (selectedNodeId == nodeIdInt);

        // header color
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
        bool isEvent = tmpl && tmpl->isEvent;
        Color headerColor = isEvent ? Color{120, 60, 30, 255} : Color{50, 60, 80, 255};
        Color bodyColor   = {38, 40, 45, 255};
        Color borderColor = isSelected ? Color{100, 160, 230, 255} : Color{70, 70, 80, 255};

        DrawRectangleRec({nodeX, nodeY, nodeWidth, headerHeight}, headerColor);
        DrawRectangleRec({nodeX, nodeY + headerHeight, nodeWidth, nodeH - headerHeight}, bodyColor);
        DrawRectangleLinesEx(nodeRect, 1.5f, borderColor);
        DrawText(node.title.c_str(), (int)nodeX + 8, (int)nodeY + 5, 13, WHITE);

        // input pins
        for (size_t i = 0; i < node.inputs.size(); i++) {
            const Pin& pin = node.inputs[i];
            float py = nodeY + headerHeight + i * pinHeight + pinHeight * 0.5f;
            Vector2 pinPos = {nodeX + pinRadius + 2, py};
            Color pinColor = GetPinColor(pin.type);

            bool filled = false;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.inputPinId == pin.id) { filled = true; break; }
            }
            if (filled) DrawCircleV(pinPos, pinRadius, pinColor);
            else        DrawCircleV(pinPos, pinRadius, {45, 45, 45, 255});
            DrawCircleLinesV(pinPos, pinRadius, pinColor);

            if (pin.type != PinType::Exec) {
                DrawText(pin.name.c_str(), (int)pinPos.x + 10, (int)py - 6, 12, LIGHTGRAY);
            }

            if (CheckCollisionPointCircle(mousePos, pinPos, pinRadius + 3) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, pinRadius + 2, {255, 255, 255, 80});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (linkFromPin == -1) StartLinkFrom(pin.id, false);
                    else TryCompleteLink(pin.id, true);
                }
            }
        }

        // output pins
        for (size_t i = 0; i < node.outputs.size(); i++) {
            const Pin& pin = node.outputs[i];
            float py = nodeY + headerHeight + i * pinHeight + pinHeight * 0.5f;
            Vector2 pinPos = {nodeX + nodeWidth - pinRadius - 2, py};
            Color pinColor = GetPinColor(pin.type);

            bool filled = false;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.outputPinId == pin.id) { filled = true; break; }
            }
            if (filled) DrawCircleV(pinPos, pinRadius, pinColor);
            else        DrawCircleV(pinPos, pinRadius, {45, 45, 45, 255});
            DrawCircleLinesV(pinPos, pinRadius, pinColor);

            if (pin.type != PinType::Exec) {
                float textW = MeasureText(pin.name.c_str(), 12);
                DrawText(pin.name.c_str(), (int)(pinPos.x - textW - 10), (int)py - 6, 12, LIGHTGRAY);
            }

            if (CheckCollisionPointCircle(mousePos, pinPos, pinRadius + 3) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, pinRadius + 2, {255, 255, 255, 80});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (linkFromPin == -1) StartLinkFrom(pin.id, true);
                    else TryCompleteLink(pin.id, false);
                }
            }
        }

        // node drag
        if (CheckCollisionPointRec(mousePos, {nodeX, nodeY, nodeWidth, headerHeight}) && CheckCollisionPointRec(mousePos, graphArea)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1) {
                selectedNodeId = nodeIdInt;
                draggingNode = true;
                nodeDragOffset = {mousePos.x - nodeX, mousePos.y - nodeY};
            }
        }

        if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            node.posX = mousePos.x - graphArea.x - scroll.x - nodeDragOffset.x;
            node.posY = mousePos.y - graphArea.y - scroll.y - nodeDragOffset.y;
            isDirty = true;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) draggingNode = false;

    EndScissorMode();

    // panning with middle mouse
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && CheckCollisionPointRec(mousePos, graphArea)) {
        panning = true;
        panStart = mousePos;
        scrollAtPanStart = scroll;
    }
    if (panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            scroll.x = scrollAtPanStart.x + (mousePos.x - panStart.x);
            scroll.y = scrollAtPanStart.y + (mousePos.y - panStart.y);
        } else {
            panning = false;
        }
    }

    // deselect on empty click
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !draggingNode) {
        bool hitNode = false;
        for (const Node& node : graph.GetNodes()) {
            float nodeX = graphArea.x + node.posX + scroll.x;
            float nodeY = graphArea.y + node.posY + scroll.y;
            float nodeH = headerHeight + std::max(node.inputs.size(), node.outputs.size()) * pinHeight + 4;
            if (CheckCollisionPointRec(mousePos, {nodeX, nodeY, nodeWidth, nodeH})) { hitNode = true; break; }
        }
        if (!hitNode) selectedNodeId = -1;
    }

    // right-click: open add node menu
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1) {
        nodeMenuOpen = true;
        nodeMenuPos = mousePos;
    }

    // delete key removes selected node
    if (IsKeyPressed(KEY_DELETE) && selectedNodeId != -1) DeleteSelected();

    // ctrl+s saves
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && isDirty) {
        graph.SaveToFile(filePath);
        isDirty = false;
    }
}

void StargraphEditorPanel::RenderNodeContextMenu() {
    if (!nodeMenuOpen) return;

    const std::unordered_map<std::string, NodeTemplate>& templates = NodeRegistry::Get().GetAllTemplates();

    // group by category
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto& kv : templates) {
        entries.push_back({kv.second.category, kv.second.type});
    }
    std::sort(entries.begin(), entries.end());

    float menuW = 180;
    float itemH = 22;
    float menuH = entries.size() * itemH + 8;
    Rectangle popup = {nodeMenuPos.x, nodeMenuPos.y, menuW, menuH};

    // keep in bounds
    if (popup.x + popup.width > bounds.x + bounds.width - sidebarWidth)
        popup.x = bounds.x + bounds.width - sidebarWidth - popup.width;
    if (popup.y + popup.height > bounds.y + bounds.height)
        popup.y = bounds.y + bounds.height - popup.height;

    DrawRectangleRec(popup, {55, 55, 60, 255});
    DrawRectangleLinesEx(popup, 1, {90, 90, 100, 255});

    std::string lastCategory;
    float y = popup.y + 4;
    for (const auto& entry : entries) {
        if (entry.first != lastCategory && !lastCategory.empty()) {
            DrawLineV({popup.x + 4, y}, {popup.x + popup.width - 4, y}, {90, 90, 90, 255});
            y += 4;
        }
        lastCategory = entry.first;

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        Vector2 mousePos = GetMousePosition();
        if (CheckCollisionPointRec(mousePos, btn)) {
            DrawRectangleRec(btn, {70, 70, 85, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Rectangle graphArea = {bounds.x, bounds.y + 25, bounds.width - sidebarWidth, bounds.height - 25};
                Node* node = graph.CreateNode(entry.second);
                node->posX = nodeMenuPos.x - graphArea.x - scroll.x;
                node->posY = nodeMenuPos.y - graphArea.y - scroll.y;
                isDirty = true;
                nodeMenuOpen = false;
            }
        }
        DrawText(entry.second.c_str(), (int)btn.x + 6, (int)btn.y + 4, 12, WHITE);
        y += itemH;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), popup)) {
        nodeMenuOpen = false;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        nodeMenuOpen = false;
    }
}

void StargraphEditorPanel::RenderSidebar() {
    Rectangle sidebar = {bounds.x + bounds.width - sidebarWidth, bounds.y + 25, sidebarWidth, bounds.height - 25};
    DrawRectangleRec(sidebar, {35, 35, 38, 255});
    DrawRectangleLinesEx(sidebar, 1, {60, 60, 65, 255});

    float y = sidebar.y + 8;
    DrawText("Variables", (int)sidebar.x + 8, (int)y, 13, LIGHTGRAY);
    y += 20;

    for (const Variable& var : graph.GetVariables()) {
        DrawRectangleRec({sidebar.x + 4, y, sidebarWidth - 8, 18}, {48, 48, 52, 255});
        Color varColor = GetPinColor(var.type);
        DrawRectangleRec({sidebar.x + 4, y + 4, 10, 10}, varColor);
        DrawText(var.name.c_str(), (int)sidebar.x + 20, (int)y + 2, 12, WHITE);
        y += 22;
    }

    y += 8;
    DrawLine((int)sidebar.x + 4, (int)y, (int)(sidebar.x + sidebarWidth - 4), (int)y, {60, 60, 65, 255});
    y += 8;

    DrawText("Graph", (int)sidebar.x + 8, (int)y, 13, LIGHTGRAY);
    y += 20;
    std::string classText = "Class: " + graph.parentClass;
    DrawText(classText.c_str(), (int)sidebar.x + 8, (int)y, 12, GRAY);
    y += 18;

    if (selectedNodeId != -1) {
        y += 8;
        DrawLine((int)sidebar.x + 4, (int)y, (int)(sidebar.x + sidebarWidth - 4), (int)y, {60, 60, 65, 255});
        y += 8;
        DrawText("Selected Node", (int)sidebar.x + 8, (int)y, 13, LIGHTGRAY);
        y += 20;
        for (const Node& node : graph.GetNodes()) {
            if (std::stoi(node.id) != selectedNodeId) continue;
            DrawText(node.type.c_str(), (int)sidebar.x + 8, (int)y, 12, WHITE);
            y += 18;
            for (const Pin& pin : node.inputs) {
                if (pin.type == PinType::Exec) continue;
                DrawText(pin.name.c_str(), (int)sidebar.x + 8, (int)y, 11, GRAY);
                y += 15;
            }
        }
    }

    if (isDirty) {
        DrawText("* Unsaved", (int)sidebar.x + 8, (int)(sidebar.y + sidebar.height - 20), 11, {200, 160, 60, 255});
    }
}

void StargraphEditorPanel::Render() {
    if (!isOpen) return;

    DrawRectangleRec(bounds, {28, 28, 30, 255});
    DrawRectangleLinesEx(bounds, 1, {70, 70, 75, 255});

    // tab bar
    Rectangle tabBar = {bounds.x, bounds.y, bounds.width, 25};
    DrawRectangleRec(tabBar, {40, 40, 45, 255});
    std::string tabLabel = graph.assetName + (isDirty ? " *" : "");
    DrawText(tabLabel.c_str(), (int)bounds.x + 8, (int)bounds.y + 5, 14, WHITE);

    Rectangle closeBtn = {bounds.x + bounds.width - 26, bounds.y + 2, 22, 20};
    if (GuiButton(closeBtn, "X")) {
        if (isDirty) graph.SaveToFile(filePath);
        Close();
        return;
    }

    RenderGraph();
    RenderNodeContextMenu();
    RenderSidebar();
}
