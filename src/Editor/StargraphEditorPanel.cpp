#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "ViewportPanel.h"
#include "PropertyFields.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Core/StarAsset.h"
#include "raygui.h"
#include "raymath.h"
#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>

const float StargraphEditorPanel::nodeWidth      = 190.0f;
const float StargraphEditorPanel::pinHeight      = 22.0f;
const float StargraphEditorPanel::pinRadius      = 5.0f;
const float StargraphEditorPanel::headerHeight   = 26.0f;
const float StargraphEditorPanel::leftPanelWidth = 200.0f;
const float StargraphEditorPanel::rightPanelWidth= 210.0f;
const float StargraphEditorPanel::toolbarHeight  = 30.0f;
const float StargraphEditorPanel::componentOutlinerHeight = 130.0f;
const float StargraphEditorPanel::primitiveNodeHeight = 26.0f;

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
    editingPinId = 0;
    listeningForKeyPinId = 0;
    keyPickerOpen = false;
    keyPickerPinId = 0;
    keyPickerSearch[0] = '\0';
    audioPathPicker.Close();
    scroll = {0, 0};
    didPan = false;
    variableDragActive = false;
    variableDragIndex = -1;
    componentDragActive = false;
    componentDragIndex = -1;
    return true;
}

void StargraphEditorPanel::Close() {
    isOpen = false;
    filePath.clear();
    isDirty = false;
    variableDragActive = false;
    variableDragIndex = -1;
    componentDragActive = false;
    componentDragIndex = -1;
    keyPickerOpen = false;
    keyPickerPinId = 0;
    listeningForKeyPinId = 0;
    audioPathPicker.Close();
}

Color StargraphEditorPanel::GetPinColor(PinType type) const {
    switch (type) {
        case PinType::Exec:    return {255, 255, 255, 255};
        case PinType::Boolean: return {140,  20,  20, 255};
        case PinType::Integer: return {  0, 189, 189, 255};
        case PinType::Float:   return { 90, 195,  90, 255};
        case PinType::String:  return {200,   0, 200, 255};
        case PinType::Vec3:    return {231, 191,  18, 255};
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
    if (IsPrimitiveNode(node)) return GetPrimitiveNodeHeight();
    size_t visibleInputs = 0;
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Variable_Name") continue;
        if (pin.name == "Component_Name") continue;
        visibleInputs++;
    }
    size_t rows = std::max(visibleInputs, node.outputs.size());
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Variable_Name") continue;
        if (pin.name == "Component_Name") continue;
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

bool StargraphEditorPanel::IsPrimitiveNode(const Node& node) const {
    return node.type == "Get_Variable" || node.type == "Get_Component";
}

float StargraphEditorPanel::GetPrimitiveNodeHeight() const {
    return primitiveNodeHeight;
}

Vector2 StargraphEditorPanel::GetPinPos(const Node& node, const Pin& pin) const {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};

    float nodeX = graphArea.x + node.posX * zoom + scroll.x;
    float nodeY = graphArea.y + node.posY * zoom + scroll.y;
    float nodeH = GetNodeHeight(node) * zoom;

    if (IsPrimitiveNode(node)) {
        float scaledRadius = pinRadius * zoom;
        if (pin.isInput) {
            return {nodeX + scaledRadius + 2, nodeY + nodeH * 0.5f};
        } else {
            return {nodeX + nodeWidth * zoom - scaledRadius - 6, nodeY + nodeH * 0.5f};
        }
    }

    float scaledPinHeight = pinHeight * zoom;
    float y = nodeY + headerHeight * zoom + scaledPinHeight * 0.5f;
    for (const Pin& p : node.inputs) {
        if (p.name == "Variable_Name") continue;
        if (p.name == "Component_Name") continue;
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
    RefreshWildcardPins(graph.FindNodeByPinId(outputPinId));
    RefreshWildcardPins(graph.FindNodeByPinId(inputPinId));

    for (int extraPinId : rerouteExtraPins) {
        int extraOutputPinId = linkFromIsOutput ? extraPinId : pinId;
        int extraInputPinId  = linkFromIsOutput ? pinId       : extraPinId;
        if (!PinsCompatible(extraOutputPinId, extraInputPinId)) continue;

        bool inputTaken = false;
        for (const Link& lk : graph.GetLinks()) {
            if (lk.inputPinId == extraInputPinId) { inputTaken = true; break; }
        }
        if (inputTaken) continue;

        graph.AddLink({extraOutputPinId, extraInputPinId});
        RefreshWildcardPins(graph.FindNodeByPinId(extraOutputPinId));
        RefreshWildcardPins(graph.FindNodeByPinId(extraInputPinId));
    }

    CancelLink();
}

void StargraphEditorPanel::CancelLink() {
    linkFromPin = -1;
    linkFromIsOutput = false;
    rerouteExtraPins.clear();
}

void StargraphEditorPanel::RemoveLinkFromPin(int pinId) {
    int otherPinId = -1;
    for (const Link& lk : graph.GetLinks()) {
        if (lk.inputPinId == pinId)  { otherPinId = lk.outputPinId; break; }
        if (lk.outputPinId == pinId) { otherPinId = lk.inputPinId;  break; }
    }
    graph.RemoveLink(pinId);
    isDirty = true;
    RefreshWildcardPins(graph.FindNodeByPinId(pinId));
    if (otherPinId != -1) RefreshWildcardPins(graph.FindNodeByPinId(otherPinId));
}

void StargraphEditorPanel::RemoveSpecificLink(int outputPinId, int inputPinId) {
    graph.RemoveSpecificLink(outputPinId, inputPinId);
    isDirty = true;
    RefreshWildcardPins(graph.FindNodeByPinId(outputPinId));
    RefreshWildcardPins(graph.FindNodeByPinId(inputPinId));
}

void StargraphEditorPanel::BeginReroute(int pinId) {
    std::vector<int> otherPinIds;
    for (const Link& lk : graph.GetLinks()) {
        if (lk.outputPinId == pinId) otherPinIds.push_back(lk.inputPinId);
        else if (lk.inputPinId == pinId) otherPinIds.push_back(lk.outputPinId);
    }
    if (otherPinIds.empty()) return;

    RemoveLinkFromPin(pinId);

    linkFromPin = otherPinIds[0];
    linkFromIsOutput = false;
    for (const Node& n : graph.GetNodes()) {
        for (const Pin& p : n.outputs) if (p.id == linkFromPin) { linkFromIsOutput = true; break; }
        for (const Pin& p : n.inputs)  if (p.id == linkFromPin) { linkFromIsOutput = false; break; }
    }

    rerouteExtraPins.assign(otherPinIds.begin() + 1, otherPinIds.end());
}

void StargraphEditorPanel::RefreshWildcardPins(Node* node) {
    if (!node) return;
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node->type);
    if (!tmpl || !tmpl->createPins) return;

    Node blank;
    tmpl->createPins(&blank);

    auto isWildcardInTemplate = [](const std::vector<Pin>& templatePins, const std::string& name) {
        for (const Pin& p : templatePins) if (p.name == name) return p.type == PinType::Wildcard;
        return false;
    };

    bool anyWildcard = false;
    for (Pin& pin : node->inputs) {
        if (isWildcardInTemplate(blank.inputs, pin.name)) { pin.type = PinType::Wildcard; anyWildcard = true; }
    }
    for (Pin& pin : node->outputs) {
        if (isWildcardInTemplate(blank.outputs, pin.name)) { pin.type = PinType::Wildcard; anyWildcard = true; }
    }
    if (!anyWildcard) return;

    PinType resolved = PinType::None;
    for (const Pin& pin : node->inputs) {
        if (pin.type != PinType::Wildcard) continue;
        for (const Link& lk : graph.GetLinks()) {
            if (lk.inputPinId != pin.id) continue;
            if (const Node* fromNode = graph.FindNodeByPinId(lk.outputPinId)) {
                for (const Pin& p : fromNode->outputs) {
                    if (p.id == lk.outputPinId && p.type != PinType::Wildcard) resolved = p.type;
                }
            }
        }
    }
    if (resolved == PinType::None) {
        for (const Pin& pin : node->outputs) {
            if (pin.type != PinType::Wildcard) continue;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.outputPinId != pin.id) continue;
                if (const Node* toNode = graph.FindNodeByPinId(lk.inputPinId)) {
                    for (const Pin& p : toNode->inputs) {
                        if (p.id == lk.inputPinId && p.type != PinType::Wildcard) resolved = p.type;
                    }
                }
            }
        }
    }
    if (resolved == PinType::None) return;

    for (Pin& pin : node->inputs)  if (pin.type == PinType::Wildcard) pin.type = resolved;
    for (Pin& pin : node->outputs) if (pin.type == PinType::Wildcard) pin.type = resolved;
}

struct ClipLink {
    int fromNode;
    std::string fromPin;
    int toNode;
    std::string toPin;
};

void StargraphEditorPanel::CopySelectedNodes() {
    std::vector<int> nodesToCopy = selectedNodeIds;
    if (selectedNodeId != -1 && std::find(selectedNodeIds.begin(), selectedNodeIds.end(), selectedNodeId) == selectedNodeIds.end()) {
        nodesToCopy.push_back(selectedNodeId);
    }
    if (nodesToCopy.empty()) return;

    float minX = FLT_MAX, minY = FLT_MAX;
    for (int nodeId : nodesToCopy) {
        for (const Node& node : graph.GetNodes()) {
            if (std::stoi(node.id) == nodeId) {
                if (node.posX < minX) minX = node.posX;
                if (node.posY < minY) minY = node.posY;
                break;
            }
        }
    }

    std::string clipText = "[StargraphCopy]\n";

    for (int nodeId : nodesToCopy) {
        for (const Node& node : graph.GetNodes()) {
            if (std::stoi(node.id) != nodeId) continue;

            clipText += "[Node]\n";
            clipText += "Type=" + node.type + "\n";
            clipText += "Title=" + node.title + "\n";
            clipText += "PosX=" + std::to_string(node.posX - minX) + "\n";
            clipText += "PosY=" + std::to_string(node.posY - minY) + "\n";
            for (const Pin& pin : node.inputs) {
                if (pin.type == PinType::Exec) continue;
                clipText += "Pin." + pin.name + "=" + pin.defaultValue + "\n";
                clipText += "PinType." + pin.name + "=" + PinTypeToString(pin.type) + "\n";
            }
            for (const Pin& pin : node.outputs) {
                clipText += "OutPinType." + pin.name + "=" + PinTypeToString(pin.type) + "\n";
            }
            clipText += "\n";
            break;
        }
    }

    for (const Link& link : graph.GetLinks()) {
        int fromNodeId = -1, toNodeId = -1;
        std::string fromPinName, toPinName;
        for (const Node& node : graph.GetNodes()) {
            for (const Pin& pin : node.outputs) {
                if (pin.id == link.outputPinId) { fromNodeId = std::stoi(node.id); fromPinName = pin.name; }
            }
            for (const Pin& pin : node.inputs) {
                if (pin.id == link.inputPinId) { toNodeId = std::stoi(node.id); toPinName = pin.name; }
            }
        }
        bool fromSelected = std::find(nodesToCopy.begin(), nodesToCopy.end(), fromNodeId) != nodesToCopy.end();
        bool toSelected = std::find(nodesToCopy.begin(), nodesToCopy.end(), toNodeId) != nodesToCopy.end();
        if (fromSelected && toSelected) {
            int fromCopyIdx = (int)(std::find(nodesToCopy.begin(), nodesToCopy.end(), fromNodeId) - nodesToCopy.begin());
            int toCopyIdx = (int)(std::find(nodesToCopy.begin(), nodesToCopy.end(), toNodeId) - nodesToCopy.begin());
            clipText += "[Link]\n";
            clipText += "FromNode=" + std::to_string(fromCopyIdx) + "\n";
            clipText += "FromPin=" + fromPinName + "\n";
            clipText += "ToNode=" + std::to_string(toCopyIdx) + "\n";
            clipText += "ToPin=" + toPinName + "\n";
            clipText += "\n";
        }
    }

    SetClipboardText(clipText.c_str());
}

void StargraphEditorPanel::PasteNodes() {
    const char* clipText = GetClipboardText();
    if (!clipText || clipText[0] == '\0') return;

    std::string clipboard = clipText;
    if (clipboard.find("[StargraphCopy]\n") == std::string::npos) return;

    Vector2 mousePos = GetMousePosition();
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};

    float rawX = (mousePos.x - graphArea.x - scroll.x) / zoom;
    float rawY = (mousePos.y - graphArea.y - scroll.y) / zoom;
    const float gridSnap = 16.0f;

    struct ClipNode {
        std::string type;
        std::string title;
        float posX = 0, posY = 0;
        std::vector<std::pair<std::string, std::string>> inputPins;
        std::vector<std::pair<std::string, std::string>> inputPinTypes;
        std::vector<std::pair<std::string, std::string>> outputPinTypes;
    };

    std::vector<ClipNode> parsedNodes;
    std::vector<ClipLink> parsedLinks;

    std::istringstream stream(clipboard);
    std::string line;
    ClipNode currentNode;
    bool inNode = false;
    int linkFromNode = -1;
    std::string linkFromPin, linkToPin;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            if (inNode) { parsedNodes.push_back(currentNode); currentNode = ClipNode(); }
            inNode = false;
            continue;
        }
        if (line == "[Node]") { inNode = true; currentNode = ClipNode(); continue; }
        if (line == "[Link]") { inNode = false; linkFromNode = -1; linkFromPin.clear(); linkToPin.clear(); continue; }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        if (inNode) {
            if (key == "Type") currentNode.type = value;
            else if (key == "Title") currentNode.title = value;
            else if (key == "PosX") currentNode.posX = std::stof(value);
            else if (key == "PosY") currentNode.posY = std::stof(value);
            else if (key.rfind("Pin.", 0) == 0) currentNode.inputPins.emplace_back(key.substr(4), value);
            else if (key.rfind("PinType.", 0) == 0) currentNode.inputPinTypes.emplace_back(key.substr(8), value);
            else if (key.rfind("OutPinType.", 0) == 0) currentNode.outputPinTypes.emplace_back(key.substr(11), value);
        } else {
            if (key == "FromNode") linkFromNode = std::stoi(value);
            else if (key == "FromPin") linkFromPin = value;
            else if (key == "ToNode") {
                ClipLink l;
                l.fromNode = linkFromNode;
                l.fromPin = linkFromPin;
                l.toNode = std::stoi(value);
                l.toPin = linkToPin;
                parsedLinks.push_back(l);
            }
            else if (key == "ToPin") linkToPin = value;
        }
    }

    std::vector<Node*> createdNodes;
    for (const ClipNode& cn : parsedNodes) {
        Node* node = graph.CreateNode(cn.type);
        if (!node) continue;
        node->title = cn.title;
        node->posX = roundf((rawX + cn.posX) / gridSnap) * gridSnap;
        node->posY = roundf((rawY + cn.posY) / gridSnap) * gridSnap;

        for (const auto& pin : cn.inputPins) {
            for (Pin& p : node->inputs) {
                if (p.name == pin.first) p.defaultValue = pin.second;
            }
        }
        for (const auto& pt : cn.inputPinTypes) {
            for (Pin& p : node->inputs) {
                if (p.name == pt.first && p.type == PinType::Wildcard) p.type = PinTypeFromString(pt.second);
            }
        }
        for (const auto& op : cn.outputPinTypes) {
            for (Pin& p : node->outputs) {
                if (p.name == op.first && p.type == PinType::Wildcard) p.type = PinTypeFromString(op.second);
            }
        }
        createdNodes.push_back(node);
    }

    for (const ClipLink& l : parsedLinks) {
        if (l.fromNode < 0 || l.fromNode >= (int)createdNodes.size()) continue;
        if (l.toNode < 0 || l.toNode >= (int)createdNodes.size()) continue;
        Node* fromNode = createdNodes[l.fromNode];
        Node* toNode = createdNodes[l.toNode];
        int outPinId = -1, inPinId = -1;
        for (const Pin& p : fromNode->outputs) if (p.name == l.fromPin) { outPinId = p.id; break; }
        for (const Pin& p : toNode->inputs) if (p.name == l.toPin) { inPinId = p.id; break; }
        if (outPinId != -1 && inPinId != -1) graph.AddLink({outPinId, inPinId});
    }

    for (Node* node : createdNodes) {
        for (Pin& p : node->inputs) if (p.type == PinType::Wildcard) p.type = PinType::None;
        for (Pin& p : node->outputs) if (p.type == PinType::Wildcard) p.type = PinType::None;
    }

    isDirty = true;
}

void StargraphEditorPanel::DuplicateSelected() {
    std::vector<int> nodesToDup = selectedNodeIds;
    if (selectedNodeId != -1 && std::find(selectedNodeIds.begin(), selectedNodeIds.end(), selectedNodeId) == selectedNodeIds.end()) {
        nodesToDup.push_back(selectedNodeId);
    }
    if (nodesToDup.empty()) return;

    const float offset = 32.0f;
    std::vector<int> oldIds;
    std::vector<Node*> newNodes;

    for (int nodeId : nodesToDup) {
        const Node* src = nullptr;
        for (const Node& n : graph.GetNodes()) if (std::stoi(n.id) == nodeId) { src = &n; break; }
        if (!src) continue;

        Node* node = graph.CreateNode(src->type);
        if (!node) continue;
        node->title = src->title;
        node->posX = src->posX + offset;
        node->posY = src->posY + offset;

        for (const Pin& sp : src->inputs) {
            for (Pin& p : node->inputs) {
                if (p.name != sp.name) continue;
                p.defaultValue = sp.defaultValue;
                if (p.type == PinType::Wildcard) p.type = sp.type;
            }
        }
        for (const Pin& sp : src->outputs) {
            for (Pin& p : node->outputs) {
                if (p.name == sp.name && p.type == PinType::Wildcard) p.type = sp.type;
            }
        }

        oldIds.push_back(nodeId);
        newNodes.push_back(node);
    }

    for (const Link& link : graph.GetLinks()) {
        int fromNodeId = -1, toNodeId = -1;
        std::string fromPinName, toPinName;
        for (const Node& node : graph.GetNodes()) {
            for (const Pin& pin : node.outputs) if (pin.id == link.outputPinId) { fromNodeId = std::stoi(node.id); fromPinName = pin.name; }
            for (const Pin& pin : node.inputs) if (pin.id == link.inputPinId) { toNodeId = std::stoi(node.id); toPinName = pin.name; }
        }

        int fromIdx = (int)(std::find(oldIds.begin(), oldIds.end(), fromNodeId) - oldIds.begin());
        int toIdx = (int)(std::find(oldIds.begin(), oldIds.end(), toNodeId) - oldIds.begin());
        if (fromIdx >= (int)oldIds.size() || toIdx >= (int)oldIds.size()) continue;

        Node* fromDup = newNodes[fromIdx];
        Node* toDup = newNodes[toIdx];
        int outPinId = -1, inPinId = -1;
        for (const Pin& p : fromDup->outputs) if (p.name == fromPinName) { outPinId = p.id; break; }
        for (const Pin& p : toDup->inputs) if (p.name == toPinName) { inPinId = p.id; break; }
        if (outPinId != -1 && inPinId != -1) graph.AddLink({outPinId, inPinId});
    }

    selectedNodeIds.clear();
    for (Node* n : newNodes) selectedNodeIds.push_back(std::stoi(n->id));
    selectedNodeId = newNodes.empty() ? -1 : std::stoi(newNodes.back()->id);

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
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                            bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        float nx = graphArea.x + node.posX * zoom + scroll.x;
        float ny = graphArea.y + node.posY * zoom + scroll.y;
        if (CheckCollisionPointRec(pos, {nx, ny, nodeWidth * zoom, GetNodeHeight(node) * zoom})) return &node;
    }
    return nullptr;
}

void StargraphEditorPanel::SaveGraph() {
    graph.SaveToFile(filePath);
    isDirty = false;
    if (viewportPanel) viewportPanel->InvalidateClassGraph(filePath);
}

void StargraphEditorPanel::RenderToolbar() {
    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, {32, 32, 36, 255});
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), {60, 60, 65, 255});

    float btnY = toolbar.y + 4;
    float btnLeft = bounds.x + 8;

    Rectangle saveBtn = {btnLeft, btnY, 50, 22};
    if (GuiButton(saveBtn, "Save")) {
        SaveGraph();
    }
    btnLeft += 54;

    Vector2 mp = GetMousePosition();
    bool canStop = !varValueSnapshot.empty();

    Rectangle selfBtn = {btnLeft, btnY, 48, 22};
    bool selfHovered = CheckCollisionPointRec(mp, selfBtn);
    Color selfColor = showSelfActorDetails ? Color{70, 130, 200, 255} : Color{50, 100, 160, 255};
    DrawRectangleRec(selfBtn, selfHovered ? Color{80, 150, 230, 255} : selfColor);
    DrawText("Self", (int)selfBtn.x + 7, (int)selfBtn.y + 4, 13, WHITE);

    float btnRight = bounds.x + bounds.width - 4;

    Rectangle closeBtn = {btnRight - 22, btnY, 22, 22};
    if (GuiButton(closeBtn, "X")) {
        if (isDirty) SaveGraph();
        Close();
        return;
    }
    btnRight -= 26;

    Rectangle stopBtn = {btnRight - 48, btnY, 48, 22};
    if (canStop) {
        bool stopHovered = CheckCollisionPointRec(mp, stopBtn);
        Color stopColor = stopHovered ? Color{160, 40, 40, 255} : Color{130, 30, 30, 255};
        DrawRectangleRec(stopBtn, stopColor);
        DrawText("STOP", (int)stopBtn.x + 7, (int)stopBtn.y + 4, 13, WHITE);
        btnRight -= 52;
    }

    Rectangle playBtn = {btnRight - 48, btnY, 48, 22};
    if (!canStop) {
        bool playHovered = CheckCollisionPointRec(mp, playBtn);
        DrawRectangleRec(playBtn, playHovered ? Color{40, 160, 75, 255} : Color{30, 130, 60, 255});
        DrawText("PLAY", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);
        btnRight -= 52;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mp, selfBtn)) {
            showSelfActorDetails = !showSelfActorDetails;
            if (showSelfActorDetails) {
                selectedNodeId = -1;
                selectedNodeIds.clear();
                selectedVariableIndex = -1;
                selectedComponentIndex = -1;
            }
        }
        if (canStop && CheckCollisionPointRec(mp, stopBtn)) {
            for (size_t i = 0; i < graph.GetVariables().size() && i < varValueSnapshot.size(); i++) {
                graph.GetVariables()[i].value = varValueSnapshot[i];
            }
            varValueSnapshot.clear();
        }
        if (!canStop && CheckCollisionPointRec(mp, playBtn)) {
            varValueSnapshot.clear();
            for (const Variable& v : graph.GetVariables()) {
                varValueSnapshot.push_back(v.value);
            }
            SaveGraph();
            GraphExecutor::Run(graph, "Event_BeginPlay");
        }
    }
}

void StargraphEditorPanel::RenderComponentOutliner(Rectangle area) {
    DrawRectangleRec(area, {28, 28, 31, 255});
    DrawRectangleLinesEx(area, 1, {55, 55, 60, 255});

    Vector2 mousePos = GetMousePosition();
    float y = area.y + 6;

    DrawText("Components", (int)area.x + 8, (int)y, 12, {160, 160, 170, 255});

    // Add Component button.
    Rectangle addBtn = {area.x + area.width - 50, y - 2, 44, 16};
    if (GuiButton(addBtn, "+ Add")) {
        addComponentMenuOpen = !addComponentMenuOpen;
        addComponentMenuPos  = {addBtn.x, addBtn.y + addBtn.height};
    }

    y += 18;
    DrawLine((int)area.x + 4, (int)y, (int)(area.x + area.width - 4), (int)y, {55, 55, 60, 255});
    y += 6;

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    float listBottom = area.y + area.height - 4;

    for (size_t i = 0; i < comps.size() && y + 18 <= listBottom; i++) {
        Rectangle row = {area.x + 4, y, area.width - 8, 18};
        bool isSelected = (selectedComponentIndex == (int)i);

        DrawRectangleRec(row, isSelected ? Color{55, 60, 80, 255} : Color{38, 38, 42, 255});
        DrawRectangleRec({area.x + 8, y + 4, 10, 10}, {90, 150, 210, 255});

        std::string label = comps[i].name + " (" + comps[i].componentType + ")";
        DrawText(label.c_str(), (int)area.x + 24, (int)y + 2, 11, WHITE);

        if (CheckCollisionPointRec(mousePos, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selectedComponentIndex = (int)i;
            selectedNodeId = -1;
            selectedNodeIds.clear();
            selectedVariableIndex = -1;
            componentDragIndex = (int)i;
            componentDragActive = false;
            componentDragPos = mousePos;
        }

        if (componentDragIndex >= 0 && componentDragIndex < (int)comps.size() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - componentDragPos.x;
            float dy = mousePos.y - componentDragPos.y;
            if (!componentDragActive && (dx*dx + dy*dy > 25)) {
                componentDragActive = true;
            }
        }

        if (isSelected && IsKeyPressed(KEY_DELETE)) {
            graph.RemoveComponent(i);
            selectedComponentIndex = -1;
            isDirty = true;
            break;
        }

        y += 20;
    }

    if (comps.empty()) {
        DrawText("No components", (int)area.x + 8, (int)y, 11, {100, 100, 108, 255});
    }

    // Add Component dropdown menu, listing the known component types.
    if (addComponentMenuOpen) {
        static const char* componentTypes[] = { "SceneComponent", "MeshComponent", "CollisionComponent", "AudioComponent", "CameraComponent" };
        float menuW = 150.0f;
        float menuH = (float)(sizeof(componentTypes) / sizeof(componentTypes[0])) * 20.0f + 4.0f;
        Rectangle menu = {addComponentMenuPos.x - menuW + 44, addComponentMenuPos.y, menuW, menuH};

        DrawRectangleRec(menu, {45, 45, 50, 255});
        DrawRectangleLinesEx(menu, 1, {80, 80, 88, 255});

        float my = menu.y + 2;
        for (const char* typeName : componentTypes) {
            Rectangle item = {menu.x + 1, my, menu.width - 2, 18};
            bool hov = CheckCollisionPointRec(mousePos, item);
            if (hov) DrawRectangleRec(item, {70, 70, 80, 255});
            DrawText(typeName, (int)item.x + 6, (int)item.y + 3, 11, WHITE);

            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ComponentDecl comp;
                comp.componentType = typeName;
                comp.name          = typeName;
                graph.AddComponent(comp);
                selectedComponentIndex = (int)graph.GetComponents().size() - 1;
                selectedNodeId = -1;
                selectedNodeIds.clear();
                selectedVariableIndex = -1;
                isDirty = true;
                addComponentMenuOpen = false;
            }
            my += 20.0f;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, menu) && !CheckCollisionPointRec(mousePos, addBtn))
            addComponentMenuOpen = false;
    }
}

void StargraphEditorPanel::RenderComponentDetails(Rectangle panel, float& y) {
    ComponentDecl& comp = graph.GetComponents()[selectedComponentIndex];
    Vector2 mp = GetMousePosition();
    bool pickerBlocksInput = componentMeshPicker.ContainsPoint(mp) || componentMaterialPicker.ContainsPoint(mp) || audioPathPicker.ContainsPoint(mp);

    if (lastComponentDetailIndex != selectedComponentIndex) {
        snprintf(compNameBuf, sizeof(compNameBuf), "%s", comp.name.c_str());
        snprintf(compPosBuf[0], 32, "%.2f", comp.posX);
        snprintf(compPosBuf[1], 32, "%.2f", comp.posY);
        snprintf(compPosBuf[2], 32, "%.2f", comp.posZ);
        snprintf(compRotBuf[0], 32, "%.2f", comp.rotX);
        snprintf(compRotBuf[1], 32, "%.2f", comp.rotY);
        snprintf(compRotBuf[2], 32, "%.2f", comp.rotZ);
        snprintf(compScaleBuf[0], 32, "%.2f", comp.scaleX);
        snprintf(compScaleBuf[1], 32, "%.2f", comp.scaleY);
        snprintf(compScaleBuf[2], 32, "%.2f", comp.scaleZ);
        snprintf(compRadiusBuf, sizeof(compRadiusBuf), "%.2f", comp.collisionRadius);
        snprintf(compHeightBuf, sizeof(compHeightBuf), "%.2f", comp.collisionHeight);
        snprintf(compSoundBuf, sizeof(compSoundBuf), "%s", comp.soundAssetPath.c_str());
        snprintf(compVolumeBuf, sizeof(compVolumeBuf), "%.2f", comp.volume);
        snprintf(compFovBuf, sizeof(compFovBuf), "%.2f", comp.cameraFov);
        lastComponentDetailIndex = selectedComponentIndex;
        componentActiveField = 0;
        componentMeshPicker.Close();
        componentMaterialPicker.Close();
        audioPathPicker.Close();
        materialSlotPickerIndex = -1;
    }

    auto textField = [&](const char* label, char* buf, size_t bufSize, int fieldId, std::string& outVal) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        if (PropertyField::StringBox(r, buf, bufSize, fieldId, componentActiveField, outVal, !pickerBlocksInput))
            isDirty = true;
        y += 22;
    };

    auto floatField = [&](const char* label, char* buf, size_t bufSize, int fieldId, float& outVal) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        if (PropertyField::FloatBox(r, buf, bufSize, fieldId, componentActiveField, outVal, !pickerBlocksInput))
            isDirty = true;
        y += 22;
    };

    auto vec3Row = [&](const char* label, char bufs[3][32], int fieldBase, float& vx, float& vy, float& vz) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {180, 200, 230, 255});
        y += 16;
        Rectangle row = {panel.x + 8, y, panel.width - 16, 18};
        if (PropertyField::Vec3Row(row, bufs[0], bufs[1], bufs[2], fieldBase, componentActiveField,
                                    vx, vy, vz, !pickerBlocksInput))
            isDirty = true;
        y += 22;
    };

    auto assetField = [&](const char* label, const std::string& current, AssetPickerPopup& picker,
                           const char* extension, std::string& outVal) {
        DrawText(label, (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        DrawRectangleRec(r, {38, 38, 42, 255});
        DrawRectangleLinesEx(r, 1, {70, 70, 80, 255});
        std::string label2 = current.empty() ? "None" : std::filesystem::path(current).stem().string();
        DrawText(label2.c_str(), (int)r.x + 4, (int)r.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !pickerBlocksInput) {
            componentMeshPicker.Close();
            componentMaterialPicker.Close();
            picker.Open({r.x, r.y + r.height}, extension);
        }
        y += 22;
        std::string picked;
        if (picker.Render(picked)) {
            outVal = picked;
            isDirty = true;
        }
    };

    DrawText(comp.componentType.c_str(), (int)panel.x + 8, (int)y, 13, WHITE);
    y += 20;

    textField("Name", compNameBuf, sizeof(compNameBuf), 1, comp.name);
    y += 4;

    vec3Row("Position", compPosBuf, 2, comp.posX, comp.posY, comp.posZ);
    y += 2;
    vec3Row("Rotation", compRotBuf, 5, comp.rotX, comp.rotY, comp.rotZ);
    y += 2;
    vec3Row("Scale", compScaleBuf, 8, comp.scaleX, comp.scaleY, comp.scaleZ);
    y += 6;

    if (comp.componentType == "MeshComponent") {
        assetField("Static Mesh", comp.meshAssetPath, componentMeshPicker, ".starasset", comp.meshAssetPath);

        std::vector<MaterialSlot> meshSlots;
        if (!comp.meshAssetPath.empty()) {
            StarAsset meshAsset;
            if (StarAsset::LoadFromFile(comp.meshAssetPath, meshAsset)) meshSlots = meshAsset.materialSlots;
        }
        if (comp.materialAssetPaths.size() != meshSlots.size()) comp.materialAssetPaths.resize(meshSlots.size());

        if (meshSlots.empty()) {
            DrawText("No material slots", (int)panel.x + 8, (int)y, 10, {130, 130, 140, 255});
            y += 18;
        } else {
            DrawText("Materials", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
            y += 14;
            for (size_t i = 0; i < meshSlots.size(); i++) {
                Rectangle rowRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
                DrawRectangleRec(rowRect, {38, 38, 42, 255});
                DrawRectangleLinesEx(rowRect, 1, {70, 70, 80, 255});
                std::string slotName = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
                DrawText(slotName.c_str(), (int)rowRect.x + 4, (int)rowRect.y + 2, 10, {150, 150, 160, 255});
                std::string matLabel = comp.materialAssetPaths[i].empty() ? "None" :
                    std::filesystem::path(comp.materialAssetPaths[i]).stem().string();
                DrawText(matLabel.c_str(), (int)(rowRect.x + rowRect.width * 0.5f), (int)rowRect.y + 2, 10, WHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rowRect) && !pickerBlocksInput) {
                    componentMeshPicker.Close();
                    componentMaterialPicker.Open({rowRect.x, rowRect.y + rowRect.height}, ".starmat");
                    materialSlotPickerIndex = (int)i;
                }
                y += PropertyField::ArrayRowHeight() + 3;
            }
            std::string pickedMat;
            if (componentMaterialPicker.Render(pickedMat)) {
                if (materialSlotPickerIndex >= 0 && materialSlotPickerIndex < (int)comp.materialAssetPaths.size())
                    comp.materialAssetPaths[materialSlotPickerIndex] = pickedMat;
                isDirty = true;
            }
        }
    } else if (comp.componentType == "CollisionComponent") {
        DrawText("Shape", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, comp.collisionShape.c_str())) {
            if (comp.collisionShape == "Box") comp.collisionShape = "Sphere";
            else if (comp.collisionShape == "Sphere") comp.collisionShape = "Capsule";
            else comp.collisionShape = "Box";
            isDirty = true;
        }
        y += 24;
        if (comp.collisionShape != "Box") {
            floatField("Radius", compRadiusBuf, sizeof(compRadiusBuf), 11, comp.collisionRadius);
        }
        if (comp.collisionShape == "Capsule") {
            floatField("Height", compHeightBuf, sizeof(compHeightBuf), 12, comp.collisionHeight);
        }
    } else if (comp.componentType == "AudioComponent") {
        DrawText("Sound Path", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        DrawRectangleRec(r, {38, 38, 42, 255});
        DrawRectangleLinesEx(r, 1, {70, 70, 80, 255});
        std::string soundLabel = comp.soundAssetPath.empty() ? "None" : std::filesystem::path(comp.soundAssetPath).stem().string();
        DrawText(soundLabel.c_str(), (int)r.x + 4, (int)r.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r) && !pickerBlocksInput) {
            audioPathPicker.Open({r.x, r.y + r.height}, ".starsound");
        }
        y += 22;
        std::string pickedSound;
        if (audioPathPicker.Render(pickedSound)) {
            comp.soundAssetPath = pickedSound;
            isDirty = true;
        }
        floatField("Volume", compVolumeBuf, sizeof(compVolumeBuf), 14, comp.volume);
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Auto Play", &comp.autoPlay)) isDirty = true;
        y += 24;
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Looping", &comp.looping)) isDirty = true;
        y += 24;
    } else if (comp.componentType == "CameraComponent") {
        floatField("FOV", compFovBuf, sizeof(compFovBuf), 15, comp.cameraFov);
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Orthographic", &comp.cameraOrthographic)) isDirty = true;
        y += 24;
    }
}

static const char* ParamTypeShortName(PinType type) {
    switch (type) {
        case PinType::Boolean: return "Bool";
        case PinType::Integer: return "Int";
        case PinType::Float:   return "Float";
        case PinType::String:  return "String";
        case PinType::Vec3:    return "Vec3";
        case PinType::Object:  return "Object";
        default:                return "Float";
    }
}

static PinType NextParamType(PinType type) {
    switch (type) {
        case PinType::Boolean: return PinType::Integer;
        case PinType::Integer: return PinType::Float;
        case PinType::Float:   return PinType::String;
        case PinType::String:  return PinType::Vec3;
        case PinType::Vec3:    return PinType::Object;
        default:                return PinType::Boolean;
    }
}

void StargraphEditorPanel::RenderCustomEventDetails(Rectangle panel, float y, Node& node) {
    int nodeId = std::stoi(node.id);
    if (lastEventDetailNodeId != nodeId) {
        snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", node.title.c_str());
        int row = 0;
        for (const Pin& pin : node.outputs) {
            if (pin.type == PinType::Exec) continue;
            if (row < 8) snprintf(eventParamNameBuf[row], 64, "%s", pin.name.c_str());
            row++;
        }
        lastEventDetailNodeId = nodeId;
        eventActiveField = 0;
    }

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;
    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    std::string titleVal = node.title;
    if (PropertyField::StringBox(nameRect, eventNameBuf, sizeof(eventNameBuf), 1, eventActiveField, titleVal)) {
        node.title = titleVal;
        isDirty = true;
    }
    y += 26;

    DrawText("Replication", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;
    static const char* replNames[] = {"Local", "Replicated", "Server Only"};
    if (GuiButton({panel.x + 8, y, panel.width - 16, 18}, replNames[(int)node.eventReplication])) {
        node.eventReplication = (ReplicationMode)(((int)node.eventReplication + 1) % 3);
        isDirty = true;
    }
    y += 24;

    if (node.eventReplication != ReplicationMode::Local) {
        if (GuiCheckBox({panel.x + 8, y, 18, 18}, "Reliable", &node.eventReliable)) isDirty = true;
        y += 24;
    }

    y += 6;
    DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, {55, 55, 60, 255});
    y += 10;
    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
    y += 18;

    std::string pinToRemove;
    int row = 0;
    for (Pin& pin : node.outputs) {
        if (pin.type == PinType::Exec) continue;

        if (row < 8) {
            Rectangle nameR = {panel.x + 8, y, panel.width - 16 - 66, 18};
            std::string pname = pin.name;
            if (PropertyField::StringBox(nameR, eventParamNameBuf[row], 64, 10 + row, eventActiveField, pname)) {
                pin.name = pname;
                isDirty = true;
            }
        }

        Rectangle typeR = {panel.x + panel.width - 54, y, 30, 18};
        if (GuiButton(typeR, ParamTypeShortName(pin.type))) {
            pin.type = NextParamType(pin.type);
            isDirty = true;
        }

        Rectangle delR = {panel.x + panel.width - 20, y, 18, 18};
        if (GuiButton(delR, "X")) pinToRemove = pin.name;

        y += 22;
        row++;
    }

    if (!pinToRemove.empty()) {
        graph.RemoveDynamicPin(node, true, pinToRemove);
        isDirty = true;
    }

    y += 6;
    if (row < 8) {
        if (GuiButton({panel.x + 8, y, panel.width - 16, 20}, "+ Add Parameter")) {
            std::string newName;
            int n = 1;
            do {
                newName = "Param" + std::to_string(n++);
                bool taken = false;
                for (const Pin& p : node.outputs) if (p.name == newName) { taken = true; break; }
                if (!taken) break;
            } while (n < 100);
            graph.AddDynamicPin(node, true, newName, PinType::Float);
            isDirty = true;
        }
    }
}

void StargraphEditorPanel::RenderCallCustomEventDetails(Rectangle panel, float y, Node& node) {
    int nodeId = std::stoi(node.id);
    if (lastEventDetailNodeId != nodeId) {
        eventNameBuf[0] = '\0';
        for (const Pin& p : node.inputs) if (p.name == "Event Name") snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", p.defaultValue.c_str());
        lastEventDetailNodeId = nodeId;
        eventActiveField = 0;
    }

    DrawText("Event Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
    y += 14;

    Pin* eventNamePin = nullptr;
    for (Pin& p : node.inputs) if (p.name == "Event Name") eventNamePin = &p;

    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    if (eventNamePin) {
        std::string val = eventNamePin->defaultValue;
        if (PropertyField::StringBox(nameRect, eventNameBuf, sizeof(eventNameBuf), 1, eventActiveField, val)) {
            eventNamePin->defaultValue = val;
            isDirty = true;
        }
    }
    y += 26;

    if (GuiButton({panel.x + 8, y, panel.width - 16, 20}, "Refresh Pins")) {
        std::string targetName = eventNamePin ? std::string(eventNamePin->defaultValue) : std::string();
        Node* targetEvent = graph.FindCustomEventByName(targetName);
        if (targetEvent) {
            std::vector<std::string> keep = {"Target", "Event Name"};
            for (const Pin& srcPin : targetEvent->outputs) {
                if (srcPin.type == PinType::Exec) continue;
                graph.AddDynamicPin(node, false, srcPin.name, srcPin.type);
                keep.push_back(srcPin.name);
            }
            std::vector<std::string> toRemove;
            for (const Pin& p : node.inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keep) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemove.push_back(p.name);
            }
            for (const std::string& n : toRemove) graph.RemoveDynamicPin(node, false, n);
            isDirty = true;
        }
    }
    y += 28;

    DrawText("Parameters", (int)panel.x + 8, (int)y, 12, {160, 160, 170, 255});
    y += 18;
    for (const Pin& pin : node.inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Target" || pin.name == "Event Name") continue;
        DrawText((pin.name + " (" + ParamTypeShortName(pin.type) + ")").c_str(), (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 16;
    }
}

void StargraphEditorPanel::RenderLeftPanel() {
    float topY = bounds.y + toolbarHeight + componentOutlinerHeight;
    Rectangle panel = {bounds.x, topY, leftPanelWidth, bounds.height - toolbarHeight - componentOutlinerHeight};
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

    sectionHeader("Graphs");
    DrawRectangleRec({panel.x + 4, y, panel.width - 8, 18}, {42, 42, 48, 255});
    DrawText(graph.assetName.c_str(), (int)panel.x + 20, (int)y + 2, 12, WHITE);
    y += 22;

    y += 8;
    sectionHeader("Functions");

    y += 8;
    sectionHeader("Variables");

    std::vector<Variable>& vars = graph.GetVariables();
    for (size_t i = 0; i < vars.size(); i++) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        bool isSelected = (selectedVariableIndex == (int)i);

        DrawRectangleRec(row, isSelected ? Color{55, 60, 80, 255} : Color{42, 42, 48, 255});
        DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(vars[i].type));

        if (renamingVariableIndex == (int)i) {
            std::string nameOut = vars[i].name;
            if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer,
                                          sizeof(varNameBuffer), 1, variableNameActiveField, nameOut)) {
                vars[i].name = nameOut;
                renamingVariableIndex = -1;
                variableNameActiveField = 0;
                isDirty = true;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                renamingVariableIndex = -1;
                variableNameActiveField = 0;
            }
        } else {
            std::string label = vars[i].name + (vars[i].isArray ? " []" : "");
            DrawText(label.c_str(), (int)panel.x + 24, (int)y + 2, 12, WHITE);

            if (CheckCollisionPointRec(mousePos, row)) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    selectedVariableIndex = (int)i;
                    selectedComponentIndex = -1;
                    selectedNodeId = -1;
                    selectedNodeIds.clear();
                    varContextMenuOpen = false;
                    variableDragIndex = (int)i;
                    variableDragActive = false;
                    variableDragPos = mousePos;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    selectedVariableIndex = (int)i;
                    selectedComponentIndex = -1;
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

        std::string nameOut = varNameBuffer;
        if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer,
                                      sizeof(varNameBuffer), 1, variableNameActiveField, nameOut)) {
            if (!nameOut.empty()) {
                Variable v;
                v.name = nameOut;
                v.type = PinType::Boolean;
                graph.AddVariable(v);
                selectedVariableIndex = (int)graph.GetVariables().size() - 1;
                isDirty = true;
            }
            addingVariable = false;
            variableNameActiveField = 0;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            addingVariable = false;
            variableNameActiveField = 0;
        }
        y += 22;
    }

    Rectangle addBtn = {panel.x + 4, y + 4, panel.width - 8, 16};
    GuiButton(addBtn, "+ Variable");
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, addBtn)) {
        snprintf(varNameBuffer, sizeof(varNameBuffer), "NewVar");
        PropertyField::ForceActivate(varNameBuffer, sizeof(varNameBuffer));
        addingVariable = true;
        variableNameActiveField = 1;
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

        Rectangle renameBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 18};
        GuiButton(renameBtn, "Rename");
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, renameBtn)) {
            snprintf(varNameBuffer, sizeof(varNameBuffer), "%s", vars[varContextMenuIndex].name.c_str());
            PropertyField::ForceActivate(varNameBuffer, sizeof(varNameBuffer));
            renamingVariableIndex = varContextMenuIndex;
            variableNameActiveField = 1;
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

void StargraphEditorPanel::RenderVariableArrayEditor(Rectangle panel, float& y, Variable& var, int& activeField) {
    if (varArrayBufs.size() != var.arrayValues.size()) {
        varArrayBufs.resize(var.arrayValues.size());
    }

    PropertyField::ArrayAction action;

    Rectangle headerRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
    PropertyField::ArrayHeader(headerRect, "Elements", (int)var.arrayValues.size(), action);
    y += PropertyField::ArrayRowHeight() + 4;

    for (size_t i = 0; i < var.arrayValues.size(); i++) {
        Rectangle rowRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
        Rectangle valueRect = PropertyField::ArrayValueRect(rowRect);
        int fieldId = 10 + (int)i * 4; // 4 id slots per element; Vec3Row needs 3

        std::string elem = var.arrayValues[i];
        bool changed = false;

        switch (var.type) {
            case PinType::Boolean: {
                bool bval = (elem == "true" || elem == "1");
                Rectangle boolRect = {valueRect.x, valueRect.y, 18, valueRect.height};
                if (PropertyField::BoolBox(boolRect, bval)) {
                    elem = bval ? "true" : "false";
                    changed = true;
                }
                DrawText(bval ? "true" : "false", (int)boolRect.x + 24, (int)boolRect.y + 2, 11, WHITE);
                break;
            }
            case PinType::Integer: {
                int ival = atoi(elem.c_str());
                if (PropertyField::IntBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                           fieldId, activeField, ival)) {
                    elem = std::to_string(ival);
                    changed = true;
                }
                break;
            }
            case PinType::Float: {
                float fval = (float)atof(elem.c_str());
                if (PropertyField::FloatBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                             fieldId, activeField, fval)) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f", fval);
                    elem = buf;
                    changed = true;
                }
                break;
            }
            case PinType::Vec3: {
                float vx = 0, vy = 0, vz = 0;
                sscanf(elem.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                if (PropertyField::Vec3Row(valueRect, varArrayBufs[i].text[0], varArrayBufs[i].text[1],
                                            varArrayBufs[i].text[2], fieldId, activeField, vx, vy, vz)) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", vx, vy, vz);
                    elem = buf;
                    changed = true;
                }
                break;
            }
            default: {
                std::string strVal = elem;
                if (PropertyField::StringBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                              fieldId, activeField, strVal, true, "(empty)")) {
                    elem = strVal;
                    changed = true;
                }
                break;
            }
        }

        if (changed) {
            var.arrayValues[i] = elem;
            isDirty = true;
        }

        PropertyField::ArrayRowControls(rowRect, (int)i, (int)var.arrayValues.size(), action);
        y += PropertyField::ArrayRowHeight() + 3;
    }

    if (action.addClicked) {
        std::string def = var.DefaultElementText();
        var.arrayValues.push_back(def);

        ArrayElemBuf buf;
        if (var.type == PinType::Vec3) {
            float vx = 0, vy = 0, vz = 0;
            sscanf(def.c_str(), "%f,%f,%f", &vx, &vy, &vz);
            snprintf(buf.text[0], sizeof(buf.text[0]), "%.2f", vx);
            snprintf(buf.text[1], sizeof(buf.text[1]), "%.2f", vy);
            snprintf(buf.text[2], sizeof(buf.text[2]), "%.2f", vz);
        } else {
            snprintf(buf.text[0], sizeof(buf.text[0]), "%s", def.c_str());
        }
        varArrayBufs.push_back(buf);
        isDirty = true;
    } else if (action.removeIndex >= 0 && action.removeIndex < (int)var.arrayValues.size()) {
        var.arrayValues.erase(var.arrayValues.begin() + action.removeIndex);
        if (action.removeIndex < (int)varArrayBufs.size())
            varArrayBufs.erase(varArrayBufs.begin() + action.removeIndex);
        isDirty = true;
        activeField = 0;
    } else if (action.moveUpIndex > 0 && action.moveUpIndex < (int)var.arrayValues.size()) {
        std::swap(var.arrayValues[action.moveUpIndex], var.arrayValues[action.moveUpIndex - 1]);
        if (action.moveUpIndex < (int)varArrayBufs.size())
            std::swap(varArrayBufs[action.moveUpIndex], varArrayBufs[action.moveUpIndex - 1]);
        isDirty = true;
        activeField = 0;
    } else if (action.moveDownIndex >= 0 && action.moveDownIndex + 1 < (int)var.arrayValues.size()) {
        std::swap(var.arrayValues[action.moveDownIndex], var.arrayValues[action.moveDownIndex + 1]);
        if (action.moveDownIndex + 1 < (int)varArrayBufs.size())
            std::swap(varArrayBufs[action.moveDownIndex], varArrayBufs[action.moveDownIndex + 1]);
        isDirty = true;
        activeField = 0;
    }
}

void StargraphEditorPanel::RenderRightPanel() {
    Rectangle panel = {bounds.x + bounds.width - rightPanelWidth, bounds.y + toolbarHeight,
                       rightPanelWidth, bounds.height - toolbarHeight};
    DrawRectangleRec(panel, {30, 30, 33, 255});
    DrawRectangleLinesEx(panel, 1, {55, 55, 60, 255});

    float y = panel.y + 8;

    if (showSelfActorDetails) {
        DrawText(graph.assetName.c_str(), (int)panel.x + 8, (int)y, 13, WHITE);
        y += 20;
        DrawText("Actor properties", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        return;
    }

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    if (selectedComponentIndex >= 0 && selectedComponentIndex < (int)comps.size()) {
        RenderComponentDetails(panel, y);
        return;
    }

    std::vector<Variable>& vars = graph.GetVariables();
    if (selectedVariableIndex >= 0 && selectedVariableIndex < (int)vars.size()) {
        Variable& var = vars[selectedVariableIndex];

        DrawText("Name", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        static char varNameEditBuf[128] = "";
        static char varValBuffer[128] = "";
        static char varIntBuf[32] = "";
        static char varFloatBuf[32] = "";
        static char varVecBuf[3][32] = {"", "", ""};
        static int lastDetailIndex = -1;
        static int activeField = 0; // 0=none 1=name 2..4=value

        auto refreshValueBuffers = [&]() {
            switch (var.type) {
                case PinType::Integer:
                    snprintf(varIntBuf, sizeof(varIntBuf), "%d", atoi(var.value.c_str()));
                    break;
                case PinType::Float:
                    snprintf(varFloatBuf, sizeof(varFloatBuf), "%.2f", atof(var.value.c_str()));
                    break;
                case PinType::Vec3: {
                    float vx = 0, vy = 0, vz = 0;
                    sscanf(var.value.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                    snprintf(varVecBuf[0], 32, "%.2f", vx);
                    snprintf(varVecBuf[1], 32, "%.2f", vy);
                    snprintf(varVecBuf[2], 32, "%.2f", vz);
                    break;
                }
                default:
                    snprintf(varValBuffer, sizeof(varValBuffer), "%s", var.value.c_str());
                    break;
            }

            varArrayBufs.resize(var.arrayValues.size());
            for (size_t i = 0; i < var.arrayValues.size(); i++) {
                const std::string& elem = var.arrayValues[i];
                switch (var.type) {
                    case PinType::Vec3: {
                        float vx = 0, vy = 0, vz = 0;
                        sscanf(elem.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                        snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%.2f", vx);
                        snprintf(varArrayBufs[i].text[1], sizeof(varArrayBufs[i].text[1]), "%.2f", vy);
                        snprintf(varArrayBufs[i].text[2], sizeof(varArrayBufs[i].text[2]), "%.2f", vz);
                        break;
                    }
                    case PinType::Integer:
                        snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%d", atoi(elem.c_str()));
                        break;
                    case PinType::Float:
                        snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%.2f", atof(elem.c_str()));
                        break;
                    default:
                        snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%s", elem.c_str());
                        break;
                }
            }
        };

        if (lastDetailIndex != selectedVariableIndex) {
            snprintf(varNameEditBuf, sizeof(varNameEditBuf), "%s", var.name.c_str());
            refreshValueBuffers();
            lastDetailIndex = selectedVariableIndex;
            activeField = 0;
        }

        Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
        std::string varName = var.name;
        if (PropertyField::StringBox(nameRect, varNameEditBuf, sizeof(varNameEditBuf), 1, activeField, varName)) {
            var.name = varName;
            isDirty = true;
        }
        y += 24;

        DrawText("Type", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        static const char* typeNames[] = {"None","Exec","Boolean","Integer","Float","String","Vec3","Object","Wildcard"};
        int typeIndex = (int)var.type;
        if (typeIndex < 0 || typeIndex > 8) typeIndex = 2;

        Rectangle typeRect = {panel.x + 8, y, panel.width - 16, 18};
        PropertyField::Style typeStyle;
        DrawRectangleRec(typeRect, typeStyle.box);
        DrawRectangleLinesEx(typeRect, 1, typeStyle.border);
        DrawText(typeNames[typeIndex], (int)typeRect.x + 4, (int)typeRect.y + 3, 11, typeStyle.text);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), typeRect)) {
            int next = typeIndex + 1;
            if (next < 2 || next > 7) next = 2;
            var.type = (PinType)next;
            switch (var.type) {
                case PinType::Boolean: var.value = "false"; break;
                case PinType::Integer: var.value = "0"; break;
                case PinType::Float:   var.value = "0.00"; break;
                case PinType::Vec3:    var.value = "0.00,0.00,0.00"; break;
                default:               var.value = ""; break;
            }
            var.arrayValues.clear();
            varArrayBufs.clear();
            refreshValueBuffers();
            isDirty = true;
            activeField = 0;
        }
        y += 24;

        DrawText("Array", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;
        {
            Rectangle arrayCheckRect = {panel.x + 8, y, 18, 18};
            bool isArr = var.isArray;
            if (PropertyField::BoolBox(arrayCheckRect, isArr)) {
                var.isArray = isArr;
                if (var.isArray) {
                    var.arrayValues.clear();
                    var.arrayValues.push_back(var.value.empty() ? var.DefaultElementText() : var.value);
                } else {
                    var.value = var.arrayValues.empty() ? var.DefaultElementText() : var.arrayValues.front();
                    var.arrayValues.clear();
                }
                varArrayBufs.clear();
                refreshValueBuffers();
                isDirty = true;
                activeField = 0;
            }
            DrawText(isArr ? "true" : "false", (int)arrayCheckRect.x + 24, (int)arrayCheckRect.y + 2, 12, WHITE);
        }
        y += 24;

        DrawText("Default Value", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        if (var.isArray) {
            RenderVariableArrayEditor(panel, y, var, activeField);
        } else if (var.type == PinType::Boolean) {
            bool bval = (var.value == "true" || var.value == "1");
            Rectangle boolRect = {panel.x + 8, y, 18, 18};
            if (PropertyField::BoolBox(boolRect, bval)) {
                var.value = bval ? "true" : "false";
                isDirty = true;
            }
            DrawText(bval ? "true" : "false", (int)boolRect.x + 24, (int)boolRect.y + 2, 12, WHITE);
        } else if (var.type == PinType::Integer) {
            Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
            int ival = atoi(var.value.c_str());
            if (PropertyField::IntBox(r, varIntBuf, sizeof(varIntBuf), 2, activeField, ival)) {
                var.value = std::to_string(ival);
                isDirty = true;
            }
        } else if (var.type == PinType::Float) {
            Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
            float fval = (float)atof(var.value.c_str());
            if (PropertyField::FloatBox(r, varFloatBuf, sizeof(varFloatBuf), 2, activeField, fval)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.2f", fval);
                var.value = buf;
                isDirty = true;
            }
        } else if (var.type == PinType::Vec3) {
            Rectangle row = {panel.x + 8, y, panel.width - 16, 18};
            float vx = 0, vy = 0, vz = 0;
            sscanf(var.value.c_str(), "%f,%f,%f", &vx, &vy, &vz);
            if (PropertyField::Vec3Row(row, varVecBuf[0], varVecBuf[1], varVecBuf[2], 2, activeField,
                                        vx, vy, vz)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", vx, vy, vz);
                var.value = buf;
                isDirty = true;
            }
        } else {
            Rectangle valRect = {panel.x + 8, y, panel.width - 16, 18};
            std::string varValue = var.value;
            if (PropertyField::StringBox(valRect, varValBuffer, sizeof(varValBuffer), 2, activeField, varValue,
                                          true, "(empty)")) {
                var.value = varValue;
                isDirty = true;
            }
        }
        y += 26;

        DrawText("Replication", (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
        y += 14;

        static const char* replNames[] = {"Local", "Replicated", "Server Only"};
        Rectangle replRect = {panel.x + 8, y, panel.width - 16, 18};
        PropertyField::Style replStyle;
        DrawRectangleRec(replRect, replStyle.box);
        DrawRectangleLinesEx(replRect, 1, replStyle.border);
        DrawText(replNames[(int)var.replication], (int)replRect.x + 4, (int)replRect.y + 3, 11, replStyle.text);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), replRect)) {
            var.replication = (ReplicationMode)(((int)var.replication + 1) % 3);
            isDirty = true;
        }
        return;
    }

    if (selectedNodeId == -1) {
        DrawText("Nothing selected", (int)panel.x + 8, (int)y, 11, {100, 100, 110, 255});
        return;
    }

    Node* selectedNode = graph.FindNodeById(selectedNodeId);
    if (selectedNode && selectedNode->type == "Custom_Event") {
        RenderCustomEventDetails(panel, y, *selectedNode);
        return;
    }
    if (selectedNode && selectedNode->type == "Call_Custom_Event") {
        RenderCallCustomEventDetails(panel, y, *selectedNode);
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
            if (pin.name == "Component_Name") continue;
            DrawText(pin.name.c_str(), (int)panel.x + 8, (int)y, 11, {160, 160, 170, 255});
            y += 14;
            if ((node.type == "Play_Sound" || node.type == "Load_Music") && pin.name == "Path") {
                Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
                DrawRectangleRec(r, {38, 38, 42, 255});
                DrawRectangleLinesEx(r, 1, {70, 70, 80, 255});
                std::string pathLabel = pin.defaultValue.empty() ? "None" : std::filesystem::path((std::string)pin.defaultValue).stem().string();
                DrawText(pathLabel.c_str(), (int)r.x + 4, (int)r.y + 3, 11, WHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r)) {
                    audioPathPicker.Open({r.x, r.y + r.height}, ".starsound");
                }
                std::string pickedPath;
                if (audioPathPicker.Render(pickedPath)) {
                    Pin* editablePin = graph.FindPin(pin.id);
                    if (editablePin) {
                        editablePin->defaultValue = pickedPath;
                        isDirty = true;
                    }
                }
                y += 22;
            } else {
                DrawText(pin.defaultValue.empty() ? "(empty)" : pin.defaultValue.c_str(),
                         (int)panel.x + 12, (int)y, 11, WHITE);
                y += 16;
            }
        }
        break;
    }
}

void StargraphEditorPanel::RenderWireInProgress() {
    if (linkFromPin == -1) return;

    Vector2 mousePos = GetMousePosition();

    std::vector<int> dragPins = {linkFromPin};
    dragPins.insert(dragPins.end(), rerouteExtraPins.begin(), rerouteExtraPins.end());

    for (int dragPinId : dragPins) {
        Vector2 fromPos = {0, 0};
        Color wireColor = {180, 180, 180, 200};

        for (const Node& node : graph.GetNodes()) {
            for (const Pin& pin : node.outputs) {
                if (pin.id == dragPinId) { fromPos = GetPinPos(node, pin); wireColor = GetPinColor(pin.type); }
            }
            for (const Pin& pin : node.inputs) {
                if (pin.id == dragPinId) { fromPos = GetPinPos(node, pin); wireColor = GetPinColor(pin.type); }
            }
        }

        DrawLineBezier(fromPos, mousePos, 2.0f, wireColor);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) CancelLink();
}

void StargraphEditorPanel::RenderGraph() {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};
    Vector2 mousePos = GetMousePosition();

    if (keyPickerOpen) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            keyPickerOpen = false;
            keyPickerPinId = 0;
        }
    } else if (listeningForKeyPinId != 0) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            listeningForKeyPinId = 0;
        } else {
            int keyCode = GetKeyPressed();
            if (keyCode != 0) {
                std::string keyName = InputKeyCodeToName(keyCode);
                if (!keyName.empty()) {
                    for (Node& n : const_cast<std::deque<Node>&>(graph.GetNodes())) {
                        for (Pin& p : n.inputs) {
                            if (p.id == listeningForKeyPinId) { p.defaultValue = keyName; isDirty = true; }
                        }
                    }
                }
                listeningForKeyPinId = 0;
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                listeningForKeyPinId = 0;
            }
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(mousePos, graphArea) && !nodeMenuOpen && !keyPickerOpen) {
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
                RemoveSpecificLink(link.outputPinId, link.inputPinId);
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

        Rectangle nodeBounds = {nx, ny, scaledNodeWidth, nh};
        Rectangle headerBounds = {nx, ny, scaledNodeWidth, scaledHeaderHeight};
        
        if (IsPrimitiveNode(node)) {
            float scaledHeight = GetPrimitiveNodeHeight() * zoom;
            nodeBounds = {nx, ny, scaledNodeWidth, scaledHeight};
            headerBounds = nodeBounds;
            
            float scaledRadius = pinRadius * zoom;
            const Pin* outputPin = nullptr;
            for (const Pin& p : node.outputs) {
                if (p.name == "Value" || p.name == "Component") { outputPin = &p; break; }
            }
            
            if (outputPin && CheckCollisionPointCircle(mousePos, {nx + scaledNodeWidth - scaledRadius - 2, ny + scaledHeight * 0.5f}, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                bool linked = false;
                for (const Link& lk : graph.GetLinks()) if (lk.outputPinId == outputPin->id) { linked = true; break; }
                DrawCircleV({nx + scaledNodeWidth - scaledRadius - 2, ny + scaledHeight * 0.5f}, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen) {
                    if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                        RemoveLinkFromPin(outputPin->id);
                    } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                        BeginReroute(outputPin->id);
                    } else if (linkFromPin == -1) {
                        linkFromPin = outputPin->id; linkFromIsOutput = true;
                    }
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && linkFromPin != -1) {
                    TryCompleteLink(outputPin->id, false);
                }
            }
            
            DrawRectangleRec(nodeBounds, {38, 40, 45, 255});
            DrawRectangleLinesEx(nodeBounds, isSelected ? 2.0f : 1.0f,
                                 isSelected ? Color{100, 160, 230, 255} : GetPinColor(outputPin ? outputPin->type : PinType::Object));
            
            if (outputPin) {
                Vector2 pinPos = {nx + scaledNodeWidth - scaledRadius - 6, ny + scaledHeight * 0.5f};
                Color pinColor = GetPinColor(outputPin->type);
                bool linked = false;
                for (const Link& lk : graph.GetLinks()) if (lk.outputPinId == outputPin->id) { linked = true; break; }
                if (linked) {
                    DrawCircleV(pinPos, scaledRadius, pinColor);
                } else {
                    DrawCircleV(pinPos, scaledRadius, {40, 40, 42, 255});
                    DrawCircleLinesV(pinPos, scaledRadius, pinColor);
                }
            }
            
            std::string displayTitle = node.title;
            for (char& c : displayTitle) if (c == '_') c = ' ';
            DrawText(displayTitle.c_str(), (int)nx + 8, (int)ny + 6, (int)(13 * zoom), WHITE);
            
            if (CheckCollisionPointRec(mousePos, nodeBounds) && CheckCollisionPointRec(mousePos, graphArea)) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen) {
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
                    selectedComponentIndex = -1;
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
            
            continue;
        }

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
            if (pin.name == "Component_Name") continue;
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
            if (pin.name == "Component_Name") {
                inputY += scaledPinHeight;
                continue;
            }

            if (pin.type == PinType::Exec) {
                inputY += scaledPinHeight;
                bool isDraggingLink = linkFromPin != -1;
                if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                    DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen) {
                        if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                            RemoveLinkFromPin(pin.id);
                        } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                            BeginReroute(pin.id);
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
                float fieldRightMax = nx + scaledNodeWidth * 0.62f;
                float fieldW = fieldRightMax - fieldX;
                if (fieldW < 40 * zoom) fieldW = 40 * zoom;
                Rectangle fieldRect = {fieldX, inputY - scaledPinHeight * 0.5f, fieldW, scaledPinHeight - 2};

                if (pin.type == PinType::Boolean) {
                    bool bval = (pin.defaultValue == "true" || pin.defaultValue == "1");
                    if (PropertyField::BoolBox({fieldRect.x, fieldRect.y + 1, fieldRect.height - 2, fieldRect.height - 2}, bval)) {
                        pin.defaultValue = bval ? "true" : "false";
                        isDirty = true;
                    }
                } else if (node.type == "Input_Key" && pin.name == "Key") {
                    float listenBtnW = 22;
                    Rectangle textRect = {fieldRect.x, fieldRect.y, fieldRect.width - listenBtnW - 4, fieldRect.height};
                    Rectangle listenBtn = {fieldRect.x + fieldRect.width - listenBtnW, fieldRect.y, listenBtnW, fieldRect.height};
                    bool listening = (listeningForKeyPinId == pin.id);

                    if (!keyPickerOpen || keyPickerPinId != pin.id) {
                        std::string display = pin.defaultValue;
                        for (char& c : display) if (c == '_') c = ' ';
                        DrawRectangleRec(textRect, {35, 35, 40, 255});
                        DrawRectangleLinesEx(textRect, 1, {60, 60, 70, 255});
                        DrawText(display.c_str(), (int)textRect.x + 6, (int)textRect.y + 5, 11, WHITE);
                    }

                    DrawRectangleRec(listenBtn, listening ? Color{160, 90, 30, 255} : Color{50, 50, 56, 255});
                    DrawRectangleLinesEx(listenBtn, 1, {80, 80, 90, 255});
                    DrawText(listening ? "?" : "...", (int)listenBtn.x + 5, (int)listenBtn.y + 3, 11,
                             listening ? WHITE : LIGHTGRAY);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (CheckCollisionPointRec(mousePos, listenBtn)) {
                            listeningForKeyPinId = pin.id;
                            editingPinId = 0;
                            keyPickerOpen = false;
                            keyPickerPinId = 0;
                        } else if (CheckCollisionPointRec(mousePos, textRect)) {
                            keyPickerPinId = pin.id;
                            keyPickerPos = {textRect.x, textRect.y + textRect.height};
                            keyPickerWidth = fieldRect.width;
                            snprintf(keyPickerSearch, sizeof(keyPickerSearch), "%s", pin.defaultValue.c_str());
                            keyPickerScroll = 0;
                            keyPickerJustOpened = true;
                            keyPickerOpen = true;
                            editingPinId = 0;
                            listeningForKeyPinId = 0;
                        }
                    }
                } else {
                    bool allowActivate = (linkFromPin == -1);
                    const char* targetPlaceholder = nullptr;
                    if (pin.name == "Target" && pin.defaultValue.empty()) {
                        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
                        if (tmpl && tmpl->category == "Actor") targetPlaceholder = "self";
                    }
                    if (PropertyField::StringBox(fieldRect, editBuffer, sizeof(editBuffer), pin.id, editingPinId,
                                                  pin.defaultValue, allowActivate, targetPlaceholder,
                                                  pin.defaultValue.c_str())) {
                        isDirty = true;
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
                        BeginReroute(pin.id);
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
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen) {
                    if (linked && IsKeyDown(KEY_LEFT_ALT)) {
                        RemoveLinkFromPin(pin.id);
                    } else if (linked && IsKeyDown(KEY_LEFT_CONTROL)) {
                        BeginReroute(pin.id);
                    } else if (linkFromPin == -1) {
                        linkFromPin = pin.id; linkFromIsOutput = true;
                    }
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && linkFromPin != -1) {
                    TryCompleteLink(pin.id, false);
                }
            }
        }

        if (CheckCollisionPointRec(mousePos, {nx, ny, scaledNodeWidth, scaledHeaderHeight}) && CheckCollisionPointRec(mousePos, graphArea)) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen) {
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
                selectedComponentIndex = -1;
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

    if (keyPickerOpen) {
        RenderKeyPickerPopup();
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
            Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                                   bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};
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

    if (componentDragActive && componentDragIndex >= 0 && componentDragIndex < (int)graph.GetComponents().size()) {
        Vector2 mp = GetMousePosition();
        const ComponentDecl& comp = graph.GetComponents()[componentDragIndex];
        std::string label = "Get: " + comp.name;
        int w = (int)MeasureText(label.c_str(), 12) + 12;
        DrawRectangleRec({mp.x + 12, mp.y - 4, (float)w, 20}, {60, 60, 65, 255});
        DrawRectangleLinesEx({mp.x + 12, mp.y - 4, (float)w, 20}, 1, {100, 100, 110, 255});
        DrawText(label.c_str(), (int)(mp.x + 18), (int)(mp.y), 12, WHITE);

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                                   bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = graph.CreateNode("Get_Component");
                node->title = "Get: " + comp.name;
                node->componentClass = comp.componentType;
                for (Pin& p : node->inputs) {
                    if (p.name == "Component_Name") p.defaultValue = comp.name;
                }
                float rawVX = (mp.x - graphArea.x - scroll.x) / zoom;
                float rawVY = (mp.y - graphArea.y - scroll.y) / zoom;
                const float gridSnap = 16.0f;
                node->posX = roundf(rawVX / gridSnap) * gridSnap;
                node->posY = roundf(rawVY / gridSnap) * gridSnap;
                isDirty = true;
            }
            componentDragActive = false;
            componentDragIndex = -1;
        }
    } else if (componentDragIndex >= 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        componentDragActive = false;
        componentDragIndex = -1;
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !draggingNode && editingPinId == 0 && !keyPickerOpen) {
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

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !keyPickerOpen) {
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

    if (IsKeyDown(KEY_LEFT_CONTROL) && editingPinId == 0) {
        if (IsKeyPressed(KEY_C)) CopySelectedNodes();
        else if (IsKeyPressed(KEY_V)) PasteNodes();
        else if (IsKeyPressed(KEY_D)) DuplicateSelected();
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && isDirty) {
        SaveGraph();
    }
}

void StargraphEditorPanel::RenderNodeContextMenu() {
    if (!nodeMenuOpen) return;

    const auto& templates = NodeRegistry::Get().GetFilteredTemplates(filterCompatibleWithPin != -1);
    std::string searchLower = nodeMenuSearch;
    for (char& c : searchLower) c = (char)tolower(c);
    for (char& c : searchLower) if (c == ' ') c = '_';

    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto& t : templates) {
        if (t.type == "Get_Variable" || t.type == "Set_Variable") continue;

        if (!searchLower.empty()) {
            std::string typeLower = t.type;
            for (char& c : typeLower) c = (char)tolower(c);
            if (typeLower.find(searchLower) == std::string::npos) continue;
        }

        if (filterCompatibleWithPin != -1) {
            const Pin* fromPin = nullptr;
            bool fromIsOutput = false;
            Node* sourceNode = nullptr;
            for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
                for (Pin& pin : node.outputs) if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = true; sourceNode = &node; break; }
                for (Pin& pin : node.inputs)  if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = false; sourceNode = &node; break; }
            }
            if (fromPin) {
                bool hasCompatible = false;
                if (fromIsOutput) {
                    for (const Pin& p : t.createPins ? [&]() { Node tmp; t.createPins(&tmp); return tmp.inputs; }() : std::vector<Pin>{})
                        if (p.type == fromPin->type || p.type == PinType::Wildcard) { hasCompatible = true; break; }
                } else {
                    for (const Pin& p : t.createPins ? [&]() { Node tmp; t.createPins(&tmp); return tmp.outputs; }() : std::vector<Pin>{})
                        if (p.type == fromPin->type || p.type == PinType::Wildcard) { hasCompatible = true; break; }
                }

                if (hasCompatible && sourceNode && !sourceNode->componentClass.empty()) {
                    if (t.category == "Actor") {
                        hasCompatible = false;
                    } else if (!t.requiresComponentClass.empty()) {
                        hasCompatible = (t.requiresComponentClass == sourceNode->componentClass);
                    }
                }

                if (!hasCompatible) continue;
            }
        }
        entries.push_back({t.category, t.type});
    }

    for (const Node& node : graph.GetNodes()) {
        if (node.type != "Custom_Event") continue;
        std::string eventName = node.title;
        if (eventName.empty()) continue;

        std::string typeLower = "call_custom_event:" + eventName;
        for (char& c : typeLower) c = (char)tolower(c);
        if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

        entries.push_back({"Events", "Call_Custom_Event:" + eventName});
    }
    std::sort(entries.begin(), entries.end());

    if (entries.empty() && searchLower.empty()) { nodeMenuOpen = false; filterCompatibleWithPin = -1; return; }

    float menuW   = 200;
    float itemH   = 22;
    float searchH = 26;
    float categoryHeaderH = 18;
    float maxVisibleH = 300.0f;

    int categoryCount = 0;
    {
        std::string prev;
        for (const auto& entry : entries) {
            if (entry.first != prev) { categoryCount++; prev = entry.first; }
        }
    }

    float totalItemsH = (float)entries.size() * itemH + (float)categoryCount * categoryHeaderH + 10;
    float listH   = (totalItemsH < maxVisibleH) ? totalItemsH : maxVisibleH;
    float menuH   = searchH + listH;

    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};

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
    float y = popup.y + searchH + 4 - nodeMenuScrollOffset;
    for (const auto& entry : entries) {
        if (entry.first != lastCategory) {
            if (!lastCategory.empty()) {
                DrawLineV({popup.x + 6, y}, {popup.x + popup.width - 6, y}, {70, 70, 80, 255});
            }
            lastCategory = entry.first;
            DrawText(entry.first.c_str(), (int)popup.x + 8, (int)y + 4, 11, {150, 165, 195, 255});
            y += categoryHeaderH;
        }

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        if (CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea)) {
            DrawRectangleRec(btn, {65, 65, 85, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                std::string nodeType = entry.second;
                std::string eventName;
                if (nodeType.rfind("Call_Custom_Event:", 0) == 0) {
                    eventName = nodeType.substr(18);
                    nodeType = "Call_Custom_Event";
                }
                Node* node = graph.CreateNode(nodeType);
                if (!eventName.empty()) {
                    for (Pin& p : node->inputs) {
                        if (p.name == "Event Name") {
                            p.defaultValue = eventName;
                            break;
                        }
                    }
                    Node* targetEvent = graph.FindCustomEventByName(eventName);
                    if (targetEvent) {
                        std::vector<std::string> keep = {"Target", "Event Name"};
                        for (const Pin& srcPin : targetEvent->outputs) {
                            if (srcPin.type == PinType::Exec) continue;
                            graph.AddDynamicPin(*node, false, srcPin.name, srcPin.type);
                            keep.push_back(srcPin.name);
                        }
                        std::vector<std::string> toRemove;
                        for (Pin& p : node->inputs) {
                            if (p.type == PinType::Exec) continue;
                            bool keepIt = false;
                            for (const std::string& k : keep) if (k == p.name) { keepIt = true; break; }
                            if (!keepIt) toRemove.push_back(p.name);
                        }
                        for (const std::string& n : toRemove) graph.RemoveDynamicPin(*node, false, n);
                    }
                }
                float rawNX = (nodeMenuPos.x - graphArea.x - scroll.x) / zoom;
                float rawNY = (nodeMenuPos.y - graphArea.y - scroll.y) / zoom;
                const float gridSnap = 16.0f;
                node->posX = roundf(rawNX / gridSnap) * gridSnap;
                node->posY = roundf(rawNY / gridSnap) * gridSnap;

                if (filterCompatibleWithPin != -1) {
                    const Pin* fromPin = nullptr;
                    bool fromIsOutput = false;
                    for (const Node& n : graph.GetNodes()) {
                        for (const Pin& pin : n.outputs) if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = true; }
                        for (const Pin& pin : n.inputs)  if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = false; }
                    }
                    if (fromPin) {
                        if (fromIsOutput) {
                            for (const Pin& p : node->inputs) {
                                if (p.type == fromPin->type || p.type == PinType::Wildcard) {
                                    graph.AddLink({filterCompatibleWithPin, p.id});
                                    break;
                                }
                            }
                        } else {
                            for (const Pin& p : node->outputs) {
                                if (p.type == fromPin->type || p.type == PinType::Wildcard) {
                                    graph.AddLink({p.id, filterCompatibleWithPin});
                                    break;
                                }
                            }
                        }
                        RefreshWildcardPins(node);
                        RefreshWildcardPins(graph.FindNodeByPinId(filterCompatibleWithPin));
                    }
                }

                isDirty = true;
                nodeMenuOpen = false;
                filterCompatibleWithPin = -1;
            }
        }
        std::string entryDisplay = entry.second;
        if (entryDisplay.rfind("Call_Custom_Event:", 0) == 0) {
            entryDisplay = "Event " + entryDisplay.substr(18);
        }
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

void StargraphEditorPanel::RenderKeyPickerPopup() {
    Rectangle graphArea = {bounds.x + leftPanelWidth, bounds.y + toolbarHeight,
                           bounds.width - leftPanelWidth - rightPanelWidth, bounds.height - toolbarHeight};

    Pin* targetPin = nullptr;
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        for (Pin& pin : node.inputs) {
            if (pin.id == keyPickerPinId) { targetPin = &pin; break; }
        }
        if (targetPin) break;
    }
    if (!targetPin) { keyPickerOpen = false; keyPickerPinId = 0; return; }

    std::vector<std::string> allKeys = GetInputKeyNames();

    std::vector<int> filtered;
    std::string searchLower = keyPickerSearch;
    for (char& c : searchLower) c = (char)tolower(c);
    for (size_t i = 0; i < allKeys.size(); i++) {
        std::string display;
        for (char c : allKeys[i]) {
            display += (c == '_') ? ' ' : (char)tolower(c);
        }
        if (searchLower.empty() || display.find(searchLower) != std::string::npos) {
            filtered.push_back((int)i);
        }
    }

    float menuW = keyPickerWidth;
    float itemH = 20.0f;
    float searchH = 26.0f;
    float maxVisibleH = 220.0f;
    float listH = std::min((float)filtered.size() * itemH + 4.0f, maxVisibleH);
    float menuH = searchH + listH;

    float popupX = keyPickerPos.x;
    float popupY = keyPickerPos.y;
    if (popupX + menuW > graphArea.x + graphArea.width) popupX = graphArea.x + graphArea.width - menuW;
    if (popupY + menuH > graphArea.y + graphArea.height) popupY = graphArea.y + graphArea.height - menuH;
    if (popupX < graphArea.x) popupX = graphArea.x;
    if (popupY < graphArea.y) popupY = graphArea.y;

    Rectangle popup = {popupX, popupY, menuW, menuH};
    Vector2 mp = GetMousePosition();

    if (keyPickerJustOpened) {
        keyPickerJustOpened = false;
    } else {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, popup)) {
            keyPickerOpen = false;
            keyPickerPinId = 0;
            return;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            keyPickerOpen = false;
            keyPickerPinId = 0;
            return;
        }
    }

    DrawRectangleRec(popup, {44, 44, 50, 255});
    DrawRectangleLinesEx(popup, 1, {80, 80, 90, 255});

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    GuiTextBox(searchRect, keyPickerSearch, sizeof(keyPickerSearch), true);

    Rectangle listArea = {popup.x, popup.y + searchH, popup.width, listH};
    if (CheckCollisionPointRec(mp, listArea)) {
        float wheel = GetMouseWheelMove();
        keyPickerScroll -= wheel * itemH * 2;
    }
    float maxScroll = (float)filtered.size() * itemH + 4.0f - listH;
    if (maxScroll < 0) maxScroll = 0;
    if (keyPickerScroll < 0) keyPickerScroll = 0;
    if (keyPickerScroll > maxScroll) keyPickerScroll = maxScroll;

    float y = popup.y + searchH + 4 - keyPickerScroll;
    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    for (int idx : filtered) {
        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        bool hover = CheckCollisionPointRec(mp, btn);
        if (hover) DrawRectangleRec(btn, {65, 65, 85, 255});

        std::string display;
        for (char c : allKeys[idx]) {
            if (c == '_') display += ' ';
            else display += c;
        }
        DrawText(display.c_str(), (int)btn.x + 6, (int)btn.y + 4, 11, WHITE);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            targetPin->defaultValue = allKeys[idx];
            isDirty = true;
            keyPickerOpen = false;
            keyPickerPinId = 0;
            break;
        }
        y += itemH;
    }
    EndScissorMode();

    if ((float)filtered.size() * itemH + 4.0f > listH) {
        float scrollRatio = (maxScroll > 0) ? keyPickerScroll / maxScroll : 0;
        float barH = (listH / ((float)filtered.size() * itemH + 4.0f)) * listH;
        float barY = listArea.y + scrollRatio * (listH - barH);
        DrawRectangleRec({listArea.x + listArea.width - 4, barY, 3, barH}, {100, 100, 120, 200});
    }
}

void StargraphEditorPanel::Render() {
    if (!isOpen) return;

    DrawRectangleRec(bounds, {24, 24, 26, 255});

    RenderToolbar();
    RenderComponentOutliner({bounds.x, bounds.y + toolbarHeight, leftPanelWidth, componentOutlinerHeight});
    RenderLeftPanel();
    RenderGraph();
    RenderNodeContextMenu();
    RenderRightPanel();
}