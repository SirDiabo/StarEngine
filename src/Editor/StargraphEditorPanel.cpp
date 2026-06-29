#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "raygui.h"
#include "raymath.h"
#include <algorithm>
#include <cstring>

const float StargraphEditorPanel::nodeWidth      = 190.0f;
const float StargraphEditorPanel::pinHeight      = 22.0f;
const float StargraphEditorPanel::pinRadius      = 5.0f;
const float StargraphEditorPanel::headerHeight   = 26.0f;
const float StargraphEditorPanel::leftPanelWidth = 200.0f;
const float StargraphEditorPanel::rightPanelWidth= 210.0f;
const float StargraphEditorPanel::toolbarHeight  = 30.0f;

void StargraphEditorPanel::Initialize() {
    name = "Stargraph";
}

bool StargraphEditorPanel::Open(const std::string& path) {
    if (!graph.LoadFromFile(path)) return false;
    filePath = path;
    isOpen = true;
    isDirty = false;
    selectedNodeId = -1;
    selectedNodeIds.clear();
    boxSelecting = false;
    linkFromPin = -1;
    nodeMenuOpen = false;
    editingPinId = -1;
    scroll = {0, 0};
    didPan = false;
    variableDragActive = false;
    variableDragIndex = -1;
    return true;
}

void StargraphEditorPanel::Close() {
    isOpen = false;
    filePath.clear();
    isDirty = false;
}

Color StargraphEditorPanel::GetPinColor(PinType type) const {
    switch (type) {
        case PinType::Exec:    return {255, 255, 255, 255};
        case PinType::Boolean: return {140,  20,  20, 255};
        case PinType::Integer: return {  0, 189, 189, 255};
        case PinType::Float:   return { 90, 195,  90, 255};
        case PinType::String:  return {200,   0, 200, 255};
        case PinType::Vec3:    return {255, 165,   0, 255};
        case PinType::Object:  return {  0, 140, 255, 255};
        case PinType::Wildcard: return {180, 180, 180, 255};
        default:               return {120, 120, 120, 255};
    }
}

Color StargraphEditorPanel::GetNodeHeaderColor(const Node& node) const {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (tmpl && tmpl->isEvent) return {130, 20, 20, 255};
    return {15, 60, 120, 255};
}

float StargraphEditorPanel::GetNodeHeight(const Node& node) const {
    size_t visibleInputs = 0;
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Variable_Name") continue;
        visibleInputs++;
    }
    size_t rows = std::max(visibleInputs, node.outputs.size());
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Variable_Name") continue;
        if (pin.type != PinType::Exec && editingPinId != pin.id) {
            bool linked = false;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.inputPinId == pin.id) { linked = true; break; }
            }
            if (!linked) rows++;
        }
    }
    return headerHeight + rows * pinHeight + 6;
}

Vector2 StargraphEditorPanel::GetPinPos(const Node& node, const Pin& pin) const {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight + 25,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight - 25};

    float nodeX = graphArea.x + node.posX * zoom + scroll.x;
    float nodeY = graphArea.y + node.posY * zoom + scroll.y;

    float scaledPinHeight = pinHeight * zoom;
    float y = nodeY + headerHeight * zoom + scaledPinHeight * 0.5f;
    for (const Pin& p : node.inputs) {
        if (p.name == "Variable_Name") continue;
        if (p.id == pin.id) return {nodeX + pinRadius * zoom + 2, y};
        y += scaledPinHeight;
        if (p.type != PinType::Exec) {
            bool linked = false;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.inputPinId == p.id) { linked = true; break; }
            }
            if (!linked) y += scaledPinHeight;
        }
    }
    y = nodeY + headerHeight * zoom + scaledPinHeight * 0.5f;
    for (const Pin& p : node.outputs) {
        if (p.id == pin.id) return {nodeX + nodeWidth * zoom - pinRadius * zoom - 2, y};
        y += scaledPinHeight;
    }
    return {0, 0};
}

bool StargraphEditorPanel::PinsCompatible(int outputPinId, int inputPinId) const {
    const Pin* out = nullptr;
    const Pin* in = nullptr;
    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) if (pin.id == outputPinId) out = &pin;
        for (const Pin& pin : node.inputs)  if (pin.id == inputPinId)  in  = &pin;
    }
    if (!out || !in) return false;
    if (out->type == in->type) return true;
    if (out->type == PinType::Wildcard || in->type == PinType::Wildcard) return true;
    return false;
}

void StargraphEditorPanel::TryCompleteLink(int pinId, bool onInput) {
    if (linkFromPin == -1) return;

    int outputPinId = linkFromIsOutput ? linkFromPin : pinId;
    int inputPinId  = linkFromIsOutput ? pinId       : linkFromPin;

    if (!PinsCompatible(outputPinId, inputPinId)) { CancelLink(); return; }

    for (const Link& lk : graph.GetLinks()) {
        if (lk.inputPinId == inputPinId) { CancelLink(); return; }
    }

    graph.AddLink({outputPinId, inputPinId});
    isDirty = true;
    CancelLink();
}

void StargraphEditorPanel::CancelLink() {
    linkFromPin = -1;
    linkFromIsOutput = false;
}

void StargraphEditorPanel::RemoveLinkFromPin(int pinId) {
    graph.RemoveLink(pinId);
    isDirty = true;
}

void StargraphEditorPanel::DeleteSelected() {
    if (selectedNodeId == -1 && selectedNodeIds.empty()) return;

    if (!selectedNodeIds.empty()) {
        for (int id : selectedNodeIds) graph.RemoveNode(id);
        selectedNodeIds.clear();
        selectedNodeId = -1;
    } else {
        graph.RemoveNode(selectedNodeId);
        selectedNodeId = -1;
    }
    isDirty = true;
}

Node* StargraphEditorPanel::GetNodeAtPos(Vector2 pos) {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight + 25,
                            bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight - 25};
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        float nx = graphArea.x + node.posX * zoom + scroll.x;
        float ny = graphArea.y + node.posY * zoom + scroll.y;
        if (CheckCollisionPointRec(pos, {nx, ny, nodeWidth * zoom, GetNodeHeight(node) * zoom})) return &node;
    }
    return nullptr;
}

void StargraphEditorPanel::RenderToolbar() {
    Rectangle toolbar = {bounds.x, bounds.y + 25, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, {32, 32, 36, 255});
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), {60, 60, 65, 255});

    std::string tabLabel = graph.assetName + (isDirty ? " *" : "");
    DrawText(tabLabel.c_str(), (int)bounds.x + 8, (int)bounds.y + 30, 13, WHITE);

    float btnY = toolbar.y + 4;
    float btnRight = bounds.x + bounds.width - 4;

    Rectangle closeBtn = {btnRight - 22, btnY, 22, 22};
    if (GuiButton(closeBtn, "X")) {
        if (isDirty) graph.SaveToFile(filePath);
        Close();
        return;
    }
    btnRight -= 26;

    Rectangle saveBtn = {btnRight - 50, btnY, 50, 22};
    if (GuiButton(saveBtn, "Save")) {
        graph.SaveToFile(filePath);
        isDirty = false;
    }
    btnRight -= 54;

    Rectangle stopBtn = {btnRight - 22, btnY, 22, 22};
    DrawRectangleRec(stopBtn, {130, 30, 30, 255});
    DrawText("S", (int)stopBtn.x + 7, (int)stopBtn.y + 4, 13, WHITE);
    btnRight -= 26;

    Rectangle playBtn = {btnRight - 22, btnY, 22, 22};
    DrawRectangleRec(playBtn, {30, 130, 60, 255});
    DrawText(">", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, stopBtn)) {
            for (size_t i = 0; i < graph.GetVariables().size() && i < varValueSnapshot.size(); i++) {
                graph.GetVariables()[i].value = varValueSnapshot[i];
            }
            varValueSnapshot.clear();
        }
        if (CheckCollisionPointRec(mp, playBtn)) {
            varValueSnapshot.clear();
            for (const Variable& v : graph.GetVariables()) {
                varValueSnapshot.push_back(v.value);
            }
            graph.SaveToFile(filePath);
            isDirty = false;
            GraphExecutor::Run(graph, "Event_BeginPlay");
        }
    }
}

void StargraphEditorPanel::RenderLeftPanel() {
    Rectangle panel = {bounds.x, bounds.y + toolbarHeight + 25, leftPanelWidth, bounds.height - toolbarHeight - 25};
    DrawRectangleRec(panel, {30, 30, 33, 255});
    DrawRectangleLinesEx(panel, 1, {55, 55, 60, 255});

    Vector2 mousePos = GetMousePosition();
    float y = panel.y + 8;

    auto sectionHeader = [&](const char* label) {
        DrawText(label, (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
        y += 18;
        DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
        y += 6;
    };

    sectionHeader("Components");

    sectionHeader("Graphs");
    DrawRectangleRec({panel.x + 4, y, panel.width - 8, 18}, {42, 42, 48, 255});
    DrawText(graph.assetName.c_str(), (int)panel.x + 20, (int)y + 2, 12, WHITE);
    y += 22;

    y += 8;
    sectionHeader("Functions");

    y += 8;
    sectionHeader("Macros");

    y += 8;
    sectionHeader("Variables");

    std::vector<Variable>& vars = graph.GetVariables();
    for (size_t i = 0; i < vars.size(); i++) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        bool isSelected = (selectedVariableIndex == (int)i);

        DrawRectangleRec(row, isSelected ? Color{55, 60, 80, 255} : Color{42, 42, 48, 255});
        DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(vars[i].type));

        if (renamingVariableIndex == (int)i) {
            GuiTextBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer, sizeof(varNameBuffer), true);
            if (IsKeyPressed(KEY_ENTER)) {
                vars[i].name = varNameBuffer;
                renamingVariableIndex = -1;
                isDirty = true;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                renamingVariableIndex = -1;
            }
        } else {
            DrawText(vars[i].name.c_str(), (int)panel.x + 24, (int)y + 2, 12, WHITE);

            if (CheckCollisionPointRec(mousePos, row)) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    selectedVariableIndex = (int)i;
                    varContextMenuOpen = false;
                    variableDragIndex = (int)i;
                    variableDragActive = false;
                    variableDragPos = mousePos;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    selectedVariableIndex = (int)i;
                    varContextMenuOpen = true;
                    varContextMenuIndex = (int)i;
                }
            }
        }

        if (variableDragIndex >= 0 && variableDragIndex < (int)vars.size() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - variableDragPos.x;
            float dy = mousePos.y - variableDragPos.y;
            if (!variableDragActive && (dx*dx + dy*dy > 25)) {
                variableDragActive = true;
            }
        }

        if (isSelected && IsKeyPressed(KEY_DELETE)) {
            vars.erase(vars.begin() + i);
            selectedVariableIndex = -1;
            isDirty = true;
            break;
        }

        y += 22;
    }

    if (addingVariable) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        DrawRectangleRec(row, {42, 42, 48, 255});
        DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(PinType::Boolean));
        GuiTextBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer, sizeof(varNameBuffer), true);

        if (IsKeyPressed(KEY_ENTER) && varNameBuffer[0] != '\0') {
            Variable v;
            v.name = varNameBuffer;
            v.type = PinType::Boolean;
            graph.AddVariable(v);
            selectedVariableIndex = (int)graph.GetVariables().size() - 1;
            addingVariable = false;
            isDirty = true;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            addingVariable = false;
        }
        y += 22;
    }

    Rectangle addBtn = {panel.x + 4, y + 4, panel.width - 8, 16};
    if (GuiButton(addBtn, "+ Variable")) {
        addingVariable = true;
        varNameBuffer[0] = '\0';
        snprintf(varNameBuffer, sizeof(varNameBuffer), "NewVar");
    }

    if (varContextMenuOpen) {
        Rectangle popup = {panel.x + leftPanelWidth, 0, 110, 50};
        for (size_t i = 0; i < vars.size(); i++) {
            if ((int)i == varContextMenuIndex) {
                popup.y = panel.y + 8 + 24 + 22 + 8 + 24 + 8 + 24 + 8 + 24 + (float)i * 22 - 4;
                break;
            }
        }

        DrawRectangleRec(popup, {55, 55, 60, 255});
        DrawRectangleLinesEx(popup, 1, {80, 80, 90, 255});

        if (GuiButton({popup.x + 4, popup.y + 4, popup.width - 8, 18}, "Rename")) {
            renamingVariableIndex = varContextMenuIndex;
            snprintf(varNameBuffer, sizeof(varNameBuffer), "%s", vars[varContextMenuIndex].name.c_str());
            varContextMenuOpen = false;
        }
        if (GuiButton({popup.x + 4, popup.y + 26, popup.width - 8, 18}, "Delete")) {
            vars.erase(vars.begin() + varContextMenuIndex);
            selectedVariableIndex = -1;
            varContextMenuOpen = false;
            isDirty = true;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            varContextMenuOpen = false;
        }
    }
}

void StargraphEditorPanel::RenderRightPanel() {
    Rectangle panel = {bounds.x + bounds.width - rightPanelWidth, bounds.y + toolbarHeight + 25,
                       rightPanelWidth, bounds.height - toolbarHeight - 25};
    DrawRectangleRec(panel, {30, 30, 33, 255});
    DrawRectangleLinesEx(panel, 1, {55, 55, 60, 255});

    float y = panel.y + 8;
    DrawText("Details", (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
    y += 20;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
    y += 8;

    std::vector<Variable>& vars = graph.GetVariables();
    if (selectedVariableIndex >= 0 && selectedVariableIndex < (int)vars.size()) {
        Variable& var = vars[selectedVariableIndex];

        DrawText("Variable", (int)panel.x + 8, (int)y, 13, WHITE);
        y += 20;
        DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
        y += 8;

        DrawText("Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        static char varNameEditBuf[128] = "";
        static char varValBuffer[128] = "";
        static int lastDetailIndex = -1;
        static int activeField = 0; // 0=none 1=name 2=value

        if (lastDetailIndex != selectedVariableIndex) {
            snprintf(varNameEditBuf, sizeof(varNameEditBuf), "%s", var.name.c_str());
            snprintf(varValBuffer, sizeof(varValBuffer), "%s", var.value.c_str());
            lastDetailIndex = selectedVariableIndex;
            activeField = 0;
        }

        Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
        if (activeField == 1) {
            GuiTextBox(nameRect, varNameEditBuf, sizeof(varNameEditBuf), true);
            if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), nameRect))) {
                var.name = varNameEditBuf;
                isDirty = true;
                activeField = 0;
            }
        } else {
            DrawRectangleRec(nameRect, {38, 38, 42, 255});
            DrawRectangleLinesEx(nameRect, 1, {70, 70, 80, 255});
            DrawText(varNameEditBuf, (int)nameRect.x + 4, (int)nameRect.y + 3, 11, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), nameRect))
                activeField = 1;
        }
        y += 24;

        DrawText("Type", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        static const char* typeNames[] = {"None","Exec","Boolean","Integer","Float","String","Vec3","Object","Wildcard"};
        int typeIndex = (int)var.type;
        if (typeIndex < 0 || typeIndex > 8) typeIndex = 0;

        if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, typeNames[typeIndex])) {
            int next = (typeIndex + 1) % 9;
            if (next == 0 || next == 1) next = 2;
            var.type = (PinType)next;
            snprintf(varValBuffer, sizeof(varValBuffer), "%s", var.value.c_str());
            isDirty = true;
            activeField = 0;
        }
        y += 24;

        DrawText("Default Value", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        if (var.type == PinType::Boolean) {
            bool bval = (var.value == "true" || var.value == "1");
            bool prev = bval;
            GuiCheckBox({panel.x + 8, y, 18, 18}, bval ? "true" : "false", &bval);
            if (bval != prev) { var.value = bval ? "true" : "false"; isDirty = true; }
        } else {
            Rectangle valRect = {panel.x + 8, y, panel.width - 16, 18};
            if (activeField == 2) {
                GuiTextBox(valRect, varValBuffer, sizeof(varValBuffer), true);
                if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), valRect))) {
                    var.value = varValBuffer;
                    isDirty = true;
                    activeField = 0;
                }
            } else {
                DrawRectangleRec(valRect, {38, 38, 42, 255});
                DrawRectangleLinesEx(valRect, 1, {70, 70, 80, 255});
                DrawText(varValBuffer[0] ? varValBuffer : "(empty)", (int)valRect.x + 4, (int)valRect.y + 3, 11, varValBuffer[0] ? WHITE : Color{80,80,90,255});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), valRect))
                    activeField = 2;
            }
        }
        return;
    }

    if (selectedNodeId == -1) {
        DrawText("Nothing selected", (int)panel.x + 8, (int)y, 11, {100, 100, 110, 255});
        return;
    }

    for (const Node& node : graph.GetNodes()) {
        if (std::stoi(node.id) != selectedNodeId) continue;
        DrawText(node.title.c_str(), (int)panel.x + 8, (int)y, 13, WHITE);
        y += 20;
        DrawText(node.type.c_str(), (int)panel.x + 8, (int)y, 11, {120, 120, 130, 255});
        y += 18;
        DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
        y += 8;
        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            if (pin.name == "Variable_Name") continue;
            DrawText(pin.name.c_str(), (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
            y += 14;
            DrawText(pin.defaultValue.empty() ? "(empty)" : pin.defaultValue.c_str(),
                     (int)panel.x + 12, (int)y, 11, WHITE);
            y += 16;
        }
        break;
    }
}

void StargraphEditorPanel::RenderWireInProgress() {
    if (linkFromPin == -1) return;

    Vector2 mousePos = GetMousePosition();
    Vector2 fromPos  = {0, 0};
    Color wireColor  = {180, 180, 180, 200};

    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) {
            if (pin.id == linkFromPin) { fromPos = GetPinPos(node, pin); wireColor = GetPinColor(pin.type); }
        }
        for (const Pin& pin : node.inputs) {
            if (pin.id == linkFromPin) { fromPos = GetPinPos(node, pin); wireColor = GetPinColor(pin.type); }
        }
    }

    DrawLineBezier(fromPos, mousePos, 2.0f, wireColor);

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) CancelLink();
}

void StargraphEditorPanel::RenderGraph() {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight + 25,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight - 25};
    Vector2 mousePos = GetMousePosition();

    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(mousePos, graphArea) && !nodeMenuOpen) {
        float newZoom = zoom + wheel * 0.1f;
        newZoom = fmax(0.1f, fmin(3.0f, newZoom));
        
        Vector2 mouseBeforeZoom = {
            (mousePos.x - graphArea.x - scroll.x) / zoom,
            (mousePos.y - graphArea.y - scroll.y) / zoom
        };
        zoom = newZoom;
        scroll.x = mousePos.x - graphArea.x - mouseBeforeZoom.x * zoom;
        scroll.y = mousePos.y - graphArea.y - mouseBeforeZoom.y * zoom;
    }

    DrawRectangleRec(graphArea, {24, 24, 26, 255});

    const float gridPitch = 16.0f;
    float stepX = gridPitch * zoom;
    float stepY = gridPitch * zoom;
    float startX = fmod(scroll.x, stepX) + graphArea.x;
    float startY = fmod(scroll.y, stepY) + graphArea.y;
    for (float gx = startX; gx < graphArea.x + graphArea.width; gx += stepX) {
        for (float gy = startY; gy < graphArea.y + graphArea.height; gy += stepY) {
            DrawPixel((int)gx, (int)gy, {55, 55, 60, 255});
        }
    }

    BeginScissorMode((int)graphArea.x, (int)graphArea.y, (int)graphArea.width, (int)graphArea.height);

    for (const Link& link : graph.GetLinks()) {
        const Pin* outPin = nullptr; const Node* outNode = nullptr;
        const Pin* inPin  = nullptr; const Node* inNode  = nullptr;
        for (const Node& node : graph.GetNodes()) {
            for (const Pin& pin : node.outputs) if (pin.id == link.outputPinId) { outPin = &pin; outNode = &node; }
            for (const Pin& pin : node.inputs)  if (pin.id == link.inputPinId)  { inPin  = &pin; inNode  = &node; }
        }
        if (!outPin || !inPin || !outNode || !inNode) continue;

        Vector2 from = GetPinPos(*outNode, *outPin);
        Vector2 to   = GetPinPos(*inNode, *inPin);
        Color wireColor = GetPinColor(outPin->type);

        bool hovered = false;
        if (IsKeyDown(KEY_LEFT_ALT)) {
            Vector2 mid = {(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
            if (CheckCollisionPointCircle(mousePos, mid, 8.0f) ||
                Vector2Distance(mousePos, from) < 12.0f ||
                Vector2Distance(mousePos, to)   < 12.0f) {
                hovered = true;
            }
            
            for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
                float cx1 = from.x + (to.x - from.x) * 0.5f;
                float cy1 = from.y;
                float cx2 = cx1;
                float cy2 = to.y;
                float bx = (1-t)*(1-t)*(1-t)*from.x + 3*(1-t)*(1-t)*t*cx1 + 3*(1-t)*t*t*cx2 + t*t*t*to.x;
                float by = (1-t)*(1-t)*(1-t)*from.y + 3*(1-t)*(1-t)*t*cy1 + 3*(1-t)*t*t*cy2 + t*t*t*to.y;
                if (CheckCollisionPointCircle(mousePos, {bx, by}, 7.0f)) { hovered = true; break; }
            }
        }

        if (hovered) {
            DrawLineBezier(from, to, 3.0f, {255, 80, 80, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                graph.RemoveLink(link.outputPinId);
                isDirty = true;
                break;
            }
        } else {
            DrawLineBezier(from, to, 2.0f, wireColor);
        }
    }

    RenderWireInProgress();

    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        int nodeIdInt = std::stoi(node.id);
        float nx = graphArea.x + node.posX * zoom + scroll.x;
        float ny = graphArea.y + node.posY * zoom + scroll.y;
        float scaledNodeWidth = nodeWidth * zoom;
        float scaledHeaderHeight = headerHeight * zoom;
        float scaledPinHeight = pinHeight * zoom;
        float nh = GetNodeHeight(node) * zoom;
        bool inMulti = false;
        for (int sid : selectedNodeIds) if (sid == nodeIdInt) { inMulti = true; break; }
        bool isSelected = (selectedNodeId == nodeIdInt) || inMulti;

        DrawRectangleRec({nx, ny, scaledNodeWidth, scaledHeaderHeight}, GetNodeHeaderColor(node));
        DrawRectangleRec({nx, ny + scaledHeaderHeight, scaledNodeWidth, nh - scaledHeaderHeight}, {38, 40, 45, 255});
        DrawRectangleLinesEx({nx, ny, scaledNodeWidth, nh}, isSelected ? 2.0f : 1.0f,
                             isSelected ? Color{100, 160, 230, 255} : Color{65, 65, 75, 255});
        std::string displayTitle = node.title;
        for (char& c : displayTitle) if (c == '_') c = ' ';
        DrawText(displayTitle.c_str(), (int)nx + 8, (int)ny + 6, (int)(13 * zoom), WHITE);

        float inputY = ny + scaledHeaderHeight + scaledPinHeight * 0.5f;
        for (size_t i = 0; i < node.inputs.size(); i++) {
            Pin& pin = node.inputs[i];
            if (pin.name == "Variable_Name") continue;
            Vector2 pinPos = {nx + pinRadius * zoom + 2, inputY};
            Color pinColor = GetPinColor(pin.type);

            bool linked = false;
            for (const Link& lk : graph.GetLinks()) if (lk.inputPinId == pin.id) { linked = true; break; }

            float scaledRadius = pinRadius * zoom;

            if (linked) DrawCircleV(pinPos, scaledRadius, pinColor);
            else { DrawCircleV(pinPos, scaledRadius, {40, 40, 42, 255}); DrawCircleLinesV(pinPos, scaledRadius, pinColor); }

            if (pin.name == "Variable_Name") {
                inputY += scaledPinHeight;
                continue;
            }

            if (pin.type == PinType::Exec) {
                inputY += scaledPinHeight;
                bool isDraggingLink = linkFromPin != -1;
                if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                    DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                            RemoveLinkFromPin(pin.id);
                        } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                            int otherPinId = -1;
                            for (const Link& lk : graph.GetLinks()) {
                                if (lk.inputPinId == pin.id) { otherPinId = lk.outputPinId; break; }
                                if (lk.outputPinId == pin.id) { otherPinId = lk.inputPinId; break; }
                            }
                            if (otherPinId != -1) {
                                RemoveLinkFromPin(pin.id);
                                linkFromPin = otherPinId;
                                for (const Node& n : graph.GetNodes()) {
                                    for (const Pin& p : n.outputs) if (p.id == otherPinId) { linkFromIsOutput = true; break; }
                                    for (const Pin& p : n.inputs) if (p.id == otherPinId) { linkFromIsOutput = false; break; }
                                }
                            }
                        } else if (!isDraggingLink) {
                            linkFromPin = pin.id; linkFromIsOutput = false;
                        }
                    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isDraggingLink) {
                        TryCompleteLink(pin.id, true);
                    }
                }
                continue;
            }

            DrawText(pin.name.c_str(), (int)pinPos.x + 12, (int)inputY - 8, 12, LIGHTGRAY);
            inputY += scaledPinHeight;

            if (!linked) {
                float fieldX = nx + pinRadius * zoom + 18;
                float fieldW = scaledNodeWidth - pinRadius * zoom - 26;
                Rectangle fieldRect = {fieldX, inputY - scaledPinHeight * 0.5f, fieldW, scaledPinHeight - 2};

                if (pin.type == PinType::Boolean) {
                    bool bval = (pin.defaultValue == "true" || pin.defaultValue == "1");
                    if (GuiCheckBox({fieldRect.x, fieldRect.y + 1, fieldRect.height - 2, fieldRect.height - 2}, "", &bval)) {
                        for (Pin& mp : node.inputs) {
                            if (mp.id == pin.id) { mp.defaultValue = bval ? "true" : "false"; isDirty = true; break; }
                        }
                    }
                } else if (editingPinId == pin.id) {
                    GuiTextBox(fieldRect, editBuffer, sizeof(editBuffer), true);
                    if (IsKeyPressed(KEY_ENTER) ||
                        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, fieldRect))) {
                        for (Pin& mp : node.inputs) {
                            if (mp.id == pin.id) { mp.defaultValue = editBuffer; break; }
                        }
                        editingPinId = -1;
                        isDirty = true;
                    }
                } else {
                    DrawRectangleRec(fieldRect, {30, 30, 32, 255});
                    DrawRectangleLinesEx(fieldRect, 1, {70, 70, 80, 255});
                    const char* displayText = pin.defaultValue.empty() ? pin.name.c_str() : pin.defaultValue.c_str();
                    Color textColor = pin.defaultValue.empty() ? Color{80, 80, 90, 255} : WHITE;
                    DrawText(displayText, (int)fieldRect.x + 4, (int)fieldRect.y + 3, 11, textColor);
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && linkFromPin == -1) {
                        editingPinId = pin.id;
                        snprintf(editBuffer, sizeof(editBuffer), "%s", pin.defaultValue.c_str());
                    }
                }
                inputY += scaledPinHeight;
            }

            if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                        RemoveLinkFromPin(pin.id);
                    } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                        int otherPinId = -1;
                        for (const Link& lk : graph.GetLinks()) {
                            if (lk.inputPinId == pin.id) { otherPinId = lk.outputPinId; break; }
                            if (lk.outputPinId == pin.id) { otherPinId = lk.inputPinId; break; }
                        }
                        if (otherPinId != -1) {
                            RemoveLinkFromPin(pin.id);
                            linkFromPin = otherPinId;
                            for (const Node& n : graph.GetNodes()) {
                                for (const Pin& p : n.outputs) if (p.id == otherPinId) { linkFromIsOutput = true; break; }
                                for (const Pin& p : n.inputs) if (p.id == otherPinId) { linkFromIsOutput = false; break; }
                            }
                        }
                    } else if (linkFromPin == -1) {
                        linkFromPin = pin.id; linkFromIsOutput = false;
                    }
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && linkFromPin != -1) {
                    TryCompleteLink(pin.id, true);
                }
            }
        }

        float outputY = ny + scaledHeaderHeight + scaledPinHeight * 0.5f;
        for (size_t i = 0; i < node.outputs.size(); i++) {
            const Pin& pin = node.outputs[i];
            Vector2 pinPos = {nx + scaledNodeWidth - pinRadius * zoom - 2, outputY};
            Color pinColor = GetPinColor(pin.type);

            bool linked = false;
            for (const Link& lk : graph.GetLinks()) if (lk.outputPinId == pin.id) { linked = true; break; }

            float scaledRadius = pinRadius * zoom;

            if (linked) DrawCircleV(pinPos, scaledRadius, pinColor);
            else { DrawCircleV(pinPos, scaledRadius, {40, 40, 42, 255}); DrawCircleLinesV(pinPos, scaledRadius, pinColor); }

            if (pin.name != "Out") {
                float textW = (float)MeasureText(pin.name.c_str(), 12);
                DrawText(pin.name.c_str(), (int)(pinPos.x - textW - 12), (int)outputY - 8, 12, LIGHTGRAY);
            }
            outputY += pinHeight * zoom;

if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                        RemoveLinkFromPin(pin.id);
                    } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                        int otherPinId = -1;
                        for (const Link& lk : graph.GetLinks()) {
                            if (lk.inputPinId == pin.id) { otherPinId = lk.outputPinId; break; }
                            if (lk.outputPinId == pin.id) { otherPinId = lk.inputPinId; break; }
                        }
                        if (otherPinId != -1) {
                            RemoveLinkFromPin(pin.id);
                            linkFromPin = otherPinId;
                            for (const Node& n : graph.GetNodes()) {
                                for (const Pin& p : n.outputs) if (p.id == otherPinId) { linkFromIsOutput = true; break; }
                                for (const Pin& p : n.inputs) if (p.id == otherPinId) { linkFromIsOutput = false; break; }
                            }
                        }
                    } else if (linkFromPin == -1) {
                        linkFromPin = pin.id; linkFromIsOutput = true;
                    }
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && linkFromPin != -1) {
                    TryCompleteLink(pin.id, false);
                }
            }
        }

        if (CheckCollisionPointRec(mousePos, {nx, ny, scaledNodeWidth, scaledHeaderHeight}) && CheckCollisionPointRec(mousePos, graphArea)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1) {
                if (IsKeyDown(KEY_LEFT_CONTROL)) {
                    bool found = false;
                    for (int sid : selectedNodeIds) if (sid == nodeIdInt) { found = true; break; }
                    if (found) {
                        selectedNodeIds.erase(std::remove(selectedNodeIds.begin(), selectedNodeIds.end(), nodeIdInt), selectedNodeIds.end());
                    } else {
                        if (selectedNodeId != -1 && selectedNodeId != nodeIdInt) {
                            bool already = false;
                            for (int sid : selectedNodeIds) if (sid == selectedNodeId) { already = true; break; }
                            if (!already) selectedNodeIds.push_back(selectedNodeId);
                        }
                        selectedNodeIds.push_back(nodeIdInt);
                        selectedNodeId = nodeIdInt;
                    }
                } else {
                    bool inMulti = false;
                    for (int sid : selectedNodeIds) if (sid == nodeIdInt) { inMulti = true; break; }
                    if (!inMulti) {
                        selectedNodeIds.clear();
                    }
                    selectedNodeId = nodeIdInt;
                }
                selectedVariableIndex = -1;
                draggingNode = true;
                nodeDragOffset = {mousePos.x - nx, mousePos.y - ny};
                break;
            }
        }

        if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float rawX = (mousePos.x - graphArea.x - scroll.x - nodeDragOffset.x) / zoom;
            float rawY = (mousePos.y - graphArea.y - scroll.y - nodeDragOffset.y) / zoom;
            const float gridSnap = 16.0f;
            float snappedX = roundf(rawX / gridSnap) * gridSnap;
            float snappedY = roundf(rawY / gridSnap) * gridSnap;
            float deltaX = snappedX - node.posX;
            float deltaY = snappedY - node.posY;
            node.posX = snappedX;
            node.posY = snappedY;

            if (!selectedNodeIds.empty()) {
                for (Node& other : const_cast<std::deque<Node>&>(graph.GetNodes())) {
                    int oid = std::stoi(other.id);
                    if (oid == nodeIdInt) continue;
                    for (int sid : selectedNodeIds) {
                        if (sid == oid) {
                            other.posX = roundf((other.posX + deltaX) / gridSnap) * gridSnap;
                            other.posY = roundf((other.posY + deltaY) / gridSnap) * gridSnap;
                            break;
                        }
                    }
                }
            }
            isDirty = true;
        }
    }

    if (boxSelecting) {
        float minX = (boxSelectStart.x < boxSelectEnd.x) ? boxSelectStart.x : boxSelectEnd.x;
        float minY = (boxSelectStart.y < boxSelectEnd.y) ? boxSelectStart.y : boxSelectEnd.y;
        float maxX = (boxSelectStart.x > boxSelectEnd.x) ? boxSelectStart.x : boxSelectEnd.x;
        float maxY = (boxSelectStart.y > boxSelectEnd.y) ? boxSelectStart.y : boxSelectEnd.y;
        DrawRectangleLinesEx({minX, minY, maxX - minX, maxY - minY}, 1, {100, 160, 230, 200});
        DrawRectangleRec({minX, minY, maxX - minX, maxY - minY}, {100, 160, 230, 30});
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        draggingNode = false;
        if (linkFromPin != -1) {
            bool hitPin = false;
            for (const Node& node : graph.GetNodes()) {
                for (const Pin& pin : node.inputs) {
                    if (CheckCollisionPointCircle(mousePos, GetPinPos(node, pin), pinRadius * zoom + 4)) { hitPin = true; break; }
                }
                for (const Pin& pin : node.outputs) {
                    if (CheckCollisionPointCircle(mousePos, GetPinPos(node, pin), pinRadius * zoom + 4)) { hitPin = true; break; }
                }
                if (hitPin) break;
            }
            if (!hitPin && CheckCollisionPointRec(mousePos, graphArea)) {
                filterCompatibleWithPin = linkFromPin;
                nodeMenuOpen = true;
                nodeMenuJustOpened = true;
                nodeMenuPos = mousePos;
                nodeMenuScrollOffset = 0;
                nodeMenuSearch[0] = '\0';
            }
            CancelLink();
        }
    }

    EndScissorMode();

    if (variableDragActive && variableDragIndex >= 0 && variableDragIndex < (int)graph.GetVariables().size()) {
        Vector2 mp = GetMousePosition();
        const Variable& var = graph.GetVariables()[variableDragIndex];
        bool makingSet = IsKeyDown(KEY_LEFT_ALT);
        std::string label = (makingSet ? "Set: " : "Get: ") + var.name;
        std::string hint = makingSet ? "" : "  (Alt = Set)";
        std::string full = label + hint;
        int w = (int)MeasureText(full.c_str(), 12) + 12;
        DrawRectangleRec({mp.x + 12, mp.y - 4, (float)w, 20}, {60, 60, 65, 255});
        DrawRectangleLinesEx({mp.x + 12, mp.y - 4, (float)w, 20}, 1, {100, 100, 110, 255});
        DrawText(label.c_str(), (int)(mp.x + 18), (int)(mp.y), 12, WHITE);
        if (!hint.empty())
            DrawText(hint.c_str(), (int)(mp.x + 18 + MeasureText(label.c_str(), 12)), (int)(mp.y), 12, {130, 130, 140, 255});

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight + 25,
                                   bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight - 25};
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = nullptr;
                const Variable& var = graph.GetVariables()[variableDragIndex];

                if (IsKeyDown(KEY_LEFT_ALT)) {
                    node = graph.CreateNode("Set_Variable");
                    node->title = "Set: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                        if (p.name == "Value")         p.type = var.type;
                    }
                } else {
                    node = graph.CreateNode("Get_Variable");
                    node->title = "Get: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                    }
                    for (Pin& p : node->outputs) {
                        if (p.name == "Value") p.type = var.type;
                    }
                }

                float rawVX = (mp.x - graphArea.x - scroll.x) / zoom;
                float rawVY = (mp.y - graphArea.y - scroll.y) / zoom;
                const float gridSnap = 16.0f;
                node->posX = roundf(rawVX / gridSnap) * gridSnap;
                node->posY = roundf(rawVY / gridSnap) * gridSnap;
                isDirty = true;
            }
            variableDragActive = false;
            variableDragIndex = -1;
        }
    } else if (variableDragIndex >= 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        variableDragActive = false;
        variableDragIndex = -1;
    }

    // Pan Graph
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, graphArea)) {
        panning = true; panStart = mousePos; scrollAtPanStart = scroll; didPan = false;
    }
    if (panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            scroll.x = scrollAtPanStart.x + (mousePos.x - panStart.x);
            scroll.y = scrollAtPanStart.y + (mousePos.y - panStart.y);
            if (panStart != mousePos) {
                didPan = true;
            }
        } else panning = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !draggingNode && editingPinId == -1) {
        if (!GetNodeAtPos(mousePos)) {
            if (!IsKeyDown(KEY_LEFT_CONTROL)) {
                selectedNodeId = -1;
                selectedNodeIds.clear();
            }
            boxSelecting = true;
            boxSelectStart = mousePos;
            boxSelectEnd = mousePos;
        }
    }

    if (boxSelecting) {
        boxSelectEnd = mousePos;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            boxSelecting = false;
            float minX = (boxSelectStart.x < boxSelectEnd.x) ? boxSelectStart.x : boxSelectEnd.x;
            float minY = (boxSelectStart.y < boxSelectEnd.y) ? boxSelectStart.y : boxSelectEnd.y;
            float maxX = (boxSelectStart.x > boxSelectEnd.x) ? boxSelectStart.x : boxSelectEnd.x;
            float maxY = (boxSelectStart.y > boxSelectEnd.y) ? boxSelectStart.y : boxSelectEnd.y;
            Rectangle sel = {minX, minY, maxX - minX, maxY - minY};
            if (sel.width > 4 || sel.height > 4) {
                if (!IsKeyDown(KEY_LEFT_CONTROL)) selectedNodeIds.clear();
                for (const Node& node : graph.GetNodes()) {
                    float nx2 = graphArea.x + node.posX * zoom + scroll.x;
                    float ny2 = graphArea.y + node.posY * zoom + scroll.y;
                    float nw2 = nodeWidth * zoom;
                    float nh2 = GetNodeHeight(node) * zoom;
                    if (CheckCollisionRecs(sel, {nx2, ny2, nw2, nh2})) {
                        int nid = std::stoi(node.id);
                        bool found = false;
                        for (int sid : selectedNodeIds) if (sid == nid) { found = true; break; }
                        if (!found) selectedNodeIds.push_back(nid);
                    }
                }
                if (!selectedNodeIds.empty()) selectedNodeId = selectedNodeIds.back();
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1) {
        if (!didPan) {
        filterCompatibleWithPin = -1;
        nodeMenuOpen = true;
        nodeMenuJustOpened = true;
        nodeMenuPos = mousePos;
        nodeMenuScrollOffset = 0;
        nodeMenuSearch[0] = '\0';
        }
    }

    if (IsKeyPressed(KEY_DELETE) && selectedNodeId != -1) DeleteSelected();

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && isDirty) {
        graph.SaveToFile(filePath);
        isDirty = false;
    }
}

void StargraphEditorPanel::RenderNodeContextMenu() {
    if (!nodeMenuOpen) return;

    const auto& templates = NodeRegistry::Get().GetAllTemplates();
    std::string searchLower = nodeMenuSearch;
    for (char& c : searchLower) c = (char)tolower(c);

    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto& kv : templates) {
        if (kv.second.type == "Get_Variable" || kv.second.type == "Set_Variable") continue;

        if (!searchLower.empty()) {
            std::string typeLower = kv.second.type;
            for (char& c : typeLower) c = (char)tolower(c);
            if (typeLower.find(searchLower) == std::string::npos) continue;
        }

if (filterCompatibleWithPin != -1) {
            const Pin* fromPin = nullptr;
            bool fromIsOutput = false;
            for (const Node& node : graph.GetNodes()) {
                for (const Pin& pin : node.outputs) if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = true; }
                for (const Pin& pin : node.inputs)  if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = false; }
            }
            if (fromPin) {
                bool hasCompatible = false;
                if (fromIsOutput) {
                    for (const Pin& p : kv.second.createPins ? [&]() { Node tmp; kv.second.createPins(&tmp); return tmp.inputs; }() : std::vector<Pin>{})
                        if (p.type == fromPin->type || p.type == PinType::Wildcard) { hasCompatible = true; break; }
                } else {
                    for (const Pin& p : kv.second.createPins ? [&]() { Node tmp; kv.second.createPins(&tmp); return tmp.outputs; }() : std::vector<Pin>{})
                        if (p.type == fromPin->type || p.type == PinType::Wildcard) { hasCompatible = true; break; }
                }
                if (!hasCompatible) continue;
            }
        }
        entries.push_back({kv.second.category, kv.second.type});
    }
    std::sort(entries.begin(), entries.end());

    if (entries.empty() && searchLower.empty()) { nodeMenuOpen = false; filterCompatibleWithPin = -1; return; }

    float menuW   = 200;
    float itemH   = 22;
    float searchH = 26;
    float maxVisibleH = 300.0f;
    float totalItemsH = (float)entries.size() * itemH + 30;
    float listH   = (totalItemsH < maxVisibleH) ? totalItemsH : maxVisibleH;
    float menuH   = searchH + listH;

    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight + 25,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight - 25};

    Rectangle popup = {nodeMenuPos.x, nodeMenuPos.y, menuW, menuH};
    if (popup.x + popup.width  > graphArea.x + graphArea.width)  popup.x = graphArea.x + graphArea.width  - popup.width;
    if (popup.y + popup.height > graphArea.y + graphArea.height) popup.y = graphArea.y + graphArea.height - popup.height;

    Vector2 mp = GetMousePosition();

    if (nodeMenuJustOpened) {
        nodeMenuJustOpened = false;
    } else {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, popup)) {
            nodeMenuOpen = false; filterCompatibleWithPin = -1; return;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            nodeMenuOpen = false; filterCompatibleWithPin = -1; return;
        }
    }

    DrawRectangleRec(popup, {44, 44, 50, 255});
    DrawRectangleLinesEx(popup, 1, {80, 80, 90, 255});

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    GuiTextBox(searchRect, nodeMenuSearch, sizeof(nodeMenuSearch), true);

    Rectangle listArea = {popup.x, popup.y + searchH, popup.width, listH};
    if (CheckCollisionPointRec(mp, listArea)) {
        float wheel = GetMouseWheelMove();
        nodeMenuScrollOffset -= wheel * itemH * 2;
    }
    float maxScroll = totalItemsH - listH;
    if (maxScroll < 0) maxScroll = 0;
    if (nodeMenuScrollOffset < 0) nodeMenuScrollOffset = 0;
    if (nodeMenuScrollOffset > maxScroll) nodeMenuScrollOffset = maxScroll;

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);

    std::string lastCategory;
    float y = popup.y + searchH + 6 - nodeMenuScrollOffset;
    for (const auto& entry : entries) {
        if (entry.first != lastCategory && !lastCategory.empty()) {
            DrawLineV({popup.x + 6, y}, {popup.x + popup.width - 6, y}, {70, 70, 80, 255});
            y += 5;
        }
        lastCategory = entry.first;

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        if (CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea)) {
            DrawRectangleRec(btn, {65, 65, 85, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Node* node = graph.CreateNode(entry.second);
                float rawNX = (nodeMenuPos.x - graphArea.x - scroll.x) / zoom;
                float rawNY = (nodeMenuPos.y - graphArea.y - scroll.y) / zoom;
                const float gridSnap = 16.0f;
                node->posX = roundf(rawNX / gridSnap) * gridSnap;
                node->posY = roundf(rawNY / gridSnap) * gridSnap;
                isDirty = true;
                nodeMenuOpen = false;
                filterCompatibleWithPin = -1;
            }
        }
        std::string entryDisplay = entry.second;
        for (char& c : entryDisplay) if (c == '_') c = ' ';
        DrawText(entryDisplay.c_str(), (int)btn.x + 8, (int)btn.y + 4, 12, WHITE);
        y += itemH;
    }

    EndScissorMode();

    if (totalItemsH > listH) {
        float scrollRatio = (maxScroll > 0) ? nodeMenuScrollOffset / maxScroll : 0;
        float barH = (listH / totalItemsH) * listH;
        float barY = listArea.y + scrollRatio * (listH - barH);
        DrawRectangleRec({popup.x + popup.width - 4, barY, 3, barH}, {100, 100, 120, 200});
    }
}

void StargraphEditorPanel::Render() {
    if (!isOpen) return;

    DrawRectangleRec(bounds, {24, 24, 26, 255});

    Rectangle tabBar = {bounds.x, bounds.y, bounds.width, 25};
    DrawRectangleRec(tabBar, {36, 36, 40, 255});
    DrawLine((int)tabBar.x, (int)(tabBar.y + tabBar.height),
             (int)(tabBar.x + tabBar.width), (int)(tabBar.y + tabBar.height), {55, 55, 60, 255});

    RenderToolbar();
    RenderLeftPanel();
    RenderGraph();
    RenderNodeContextMenu();
    RenderRightPanel();
}
