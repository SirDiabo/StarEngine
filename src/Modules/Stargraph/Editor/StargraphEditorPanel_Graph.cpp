#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/ObjectKindRegistry.h"
#include "Editor/PropertyFields.h"
#include "Editor/UITheme.h"
#include "Editor/UIWidgets.h"
#include "Editor/BasicActorDefs.h"
#include "raygui.h"
#include "raymath.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <sstream>

static const std::vector<std::pair<std::string, std::string>>& BasicActorClassPickerEntries() {
    static const std::vector<std::pair<std::string, std::string>> entries = [] {
        std::vector<std::pair<std::string, std::string>> list;
        for (const BasicActorDef& def : GetBasicActorDefs()) list.emplace_back(def.label, def.kindName);
        return list;
    }();
    return entries;
}

static bool IsTimerFunctionNameNode(const std::string& type) {
    return type == "Timer_By_Function_Name" || type == "Pause_Timer_By_Function_Name" ||
           type == "Resume_Timer_By_Function_Name" || type == "Clear_Timer_By_Function_Name";
}

static std::string NodeMenuDisplayLabel(const std::string& rawType) {
    std::string display = rawType;
    if (display.rfind("Call_Custom_Event:", 0) == 0) {
        display = "Event " + display.substr(18);
    } else if (display.rfind("Input_Mapping:", 0) == 0) {
        display = "Key " + display.substr(14);
    } else if (display.rfind("Get_External_Var:", 0) == 0) {
        display = "Get " + display.substr(17);
    } else if (display.rfind("Set_External_Var:", 0) == 0) {
        display = "Set " + display.substr(17);
    } else if (display.rfind("Get_External_Component:", 0) == 0) {
        display = "Get " + display.substr(23);
    }
    for (char& c : display) if (c == '_') c = ' ';
    return display;
}

static bool IsActorClassSourceNode(const std::string& nodeType) {
    return nodeType == "Cast_To_Class" || nodeType == "Get_Actor_Of_Class" || nodeType == "For_Each_Actor_Of_Class";
}

static std::string EffectivePinObjectKind(const Node* node, const Pin& pin) {
    if (!node) return pin.objectKind;
    if (!node->componentClass.empty()) return node->componentClass;
    if (pin.type == PinType::Object && pin.isOutput && IsActorClassSourceNode(node->type) &&
        (pin.name == "As" || pin.name == "Out" || pin.name == "Current Actor")) {
        for (const Pin& in : node->inputs) {
            if (in.name == "Class" && !in.defaultValue.empty()) return in.defaultValue;
        }
    }
    return pin.objectKind;
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

float StargraphEditorPanel::GetNodeWidth(const Node& node) const {
    std::string displayTitle = node.title;
    for (char& c : displayTitle) if (c == '_') c = ' ';
    float titleW = (float)MeasureText(displayTitle.c_str(), 13);

    if (IsPrimitiveNode(node)) {
        float needed = 16.0f + titleW + pinRadius * 2.0f + 16.0f;
        return minNodeWidth > needed ? minNodeWidth : needed;
    }

    float maxInputLabelW = 0.0f;
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Variable_Name") continue;
        if (pin.name == "Component_Name") continue;
        if (pin.type == PinType::Exec) continue;
        float w = (float)MeasureText(pin.name.c_str(), 12);
        if (w > maxInputLabelW) maxInputLabelW = w;
    }

    float maxOutputLabelW = 0.0f;
    for (const Pin& pin : node.outputs) {
        float w = (float)MeasureText(pin.name.c_str(), 12);
        if (w > maxOutputLabelW) maxOutputLabelW = w;
    }

    float rowNeeded = 0.0f;
    if (maxInputLabelW > 0.0f || maxOutputLabelW > 0.0f) {
        rowNeeded = (pinRadius * 2.0f + 20.0f) + maxInputLabelW + 24.0f +
                    maxOutputLabelW + (pinRadius * 2.0f + 12.0f);
    }

    float titleNeeded = 16.0f + titleW;

    float needed = rowNeeded > titleNeeded ? rowNeeded : titleNeeded;
    return minNodeWidth > needed ? minNodeWidth : needed;
}

bool StargraphEditorPanel::IsPrimitiveNode(const Node& node) const {
    return node.type == "Get_Variable" || node.type == "Get_Component" || node.type == "Get_Saved_Variable";
}

float StargraphEditorPanel::GetPrimitiveNodeHeight() const {
    return primitiveNodeHeight;
}

Vector2 StargraphEditorPanel::GetPinPos(const Node& node, const Pin& pin) const {
    float nodeX = graphArea.x + node.posX * canvasState.zoom + canvasState.scroll.x;
    float nodeY = graphArea.y + node.posY * canvasState.zoom + canvasState.scroll.y;
    float nodeH = GetNodeHeight(node) * canvasState.zoom;

    if (IsPrimitiveNode(node)) {
        float scaledRadius = pinRadius * canvasState.zoom;
        if (pin.isInput) {
            return {nodeX + scaledRadius + 2, nodeY + nodeH * 0.5f};
        } else {
            return {nodeX + GetNodeWidth(node) * canvasState.zoom - scaledRadius - 6, nodeY + nodeH * 0.5f};
        }
    }

    float scaledPinHeight = pinHeight * canvasState.zoom;
    float y = nodeY + headerHeight * canvasState.zoom + scaledPinHeight * 0.5f;
    for (const Pin& p : node.inputs) {
        if (p.name == "Variable_Name") continue;
        if (p.name == "Component_Name") continue;
        if (p.id == pin.id) return {nodeX + pinRadius * canvasState.zoom + 2, y};
        y += scaledPinHeight;
        if (p.type != PinType::Exec) {
            bool linked = false;
            for (const Link& lk : graph.GetLinks()) {
                if (lk.inputPinId == p.id) { linked = true; break; }
            }
            if (!linked) y += scaledPinHeight;
        }
    }
    y = nodeY + headerHeight * canvasState.zoom + scaledPinHeight * 0.5f;
    for (const Pin& p : node.outputs) {
        if (p.id == pin.id) return {nodeX + GetNodeWidth(node) * canvasState.zoom - pinRadius * canvasState.zoom - 2, y};
        y += scaledPinHeight;
    }
    return {0, 0};
}

bool StargraphEditorPanel::PinsCompatible(int outputPinId, int inputPinId) const {
    const Pin* out = nullptr;
    const Pin* in = nullptr;
    const Node* outNode = nullptr;
    const Node* inNode = nullptr;
    for (const Node& node : graph.GetNodes()) {
        for (const Pin& pin : node.outputs) if (pin.id == outputPinId) { out = &pin; outNode = &node; }
        for (const Pin& pin : node.inputs)  if (pin.id == inputPinId)  { in  = &pin; inNode = &node; }
    }
    if (!out || !in) return false;
    if (out->type == PinType::Wildcard || in->type == PinType::Wildcard) return true;
    if (out->type != in->type) return false;
    if (out->type == PinType::Object) {
        return ObjectKindRegistry::Get().IsCompatible(EffectivePinObjectKind(outNode, *out), EffectivePinObjectKind(inNode, *in));
    }
    return true;
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

    Vector2 rawWorld = GraphCanvas::ScreenToWorld(canvasState, graphArea, mousePos);
    float rawX = rawWorld.x;
    float rawY = rawWorld.y;
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

        for (const auto& pt : cn.inputPinTypes) {
            bool found = false;
            for (Pin& p : node->inputs) {
                if (p.name == pt.first) { p.type = PinTypeFromString(pt.second); found = true; break; }
            }
            if (!found) graph.AddDynamicPin(*node, false, pt.first, PinTypeFromString(pt.second));
        }
        for (const auto& op : cn.outputPinTypes) {
            bool found = false;
            for (Pin& p : node->outputs) {
                if (p.name == op.first) { p.type = PinTypeFromString(op.second); found = true; break; }
            }
            if (!found) graph.AddDynamicPin(*node, true, op.first, PinTypeFromString(op.second));
        }
        for (const auto& pin : cn.inputPins) {
            for (Pin& p : node->inputs) {
                if (p.name == pin.first) p.defaultValue = pin.second;
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
            bool found = false;
            for (Pin& p : node->inputs) {
                if (p.name != sp.name) continue;
                p.defaultValue = sp.defaultValue;
                p.type = sp.type;
                found = true;
            }
            if (!found) {
                Pin* np = graph.AddDynamicPin(*node, false, sp.name, sp.type);
                if (np) np->defaultValue = sp.defaultValue;
            }
        }
        for (const Pin& sp : src->outputs) {
            bool found = false;
            for (Pin& p : node->outputs) {
                if (p.name == sp.name) { p.type = sp.type; found = true; break; }
            }
            if (!found) graph.AddDynamicPin(*node, true, sp.name, sp.type);
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
    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        float nx = graphArea.x + node.posX * canvasState.zoom + canvasState.scroll.x;
        float ny = graphArea.y + node.posY * canvasState.zoom + canvasState.scroll.y;
        if (CheckCollisionPointRec(pos, {nx, ny, GetNodeWidth(node) * canvasState.zoom, GetNodeHeight(node) * canvasState.zoom})) return &node;
    }
    return nullptr;
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

        GraphCanvas::DrawWire(fromPos, mousePos, 2.0f, wireColor);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) CancelLink();
}

void StargraphEditorPanel::RenderGraph(Rectangle area) {
    graphArea = area;
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

    GraphCanvas::UpdatePanZoom(canvasState, graphArea, mousePos, !nodeMenuOpen && !keyPickerOpen);

    GraphCanvas::DrawGrid(graphArea, canvasState.scroll, canvasState.zoom);

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

        bool hovered = IsKeyDown(KEY_LEFT_ALT) && GraphCanvas::IsPointNearWire(mousePos, from, to);

        if (hovered) {
            GraphCanvas::DrawWire(from, to, 3.0f, {255, 80, 80, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                RemoveSpecificLink(link.outputPinId, link.inputPinId);
                break;
            }
        } else {
            GraphCanvas::DrawWire(from, to, 2.0f, wireColor);
        }
    }

    RenderWireInProgress();

    for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
        int nodeIdInt = std::stoi(node.id);
        float nx = graphArea.x + node.posX * canvasState.zoom + canvasState.scroll.x;
        float ny = graphArea.y + node.posY * canvasState.zoom + canvasState.scroll.y;
        float scaledNodeWidth = GetNodeWidth(node) * canvasState.zoom;
        float scaledHeaderHeight = headerHeight * canvasState.zoom;
        float scaledPinHeight = pinHeight * canvasState.zoom;
        float nh = GetNodeHeight(node) * canvasState.zoom;
        bool inMulti = false;
        for (int sid : selectedNodeIds) if (sid == nodeIdInt) { inMulti = true; break; }
        bool isSelected = (selectedNodeId == nodeIdInt) || inMulti;

        Rectangle nodeBounds = {nx, ny, scaledNodeWidth, nh};
        Rectangle headerBounds = {nx, ny, scaledNodeWidth, scaledHeaderHeight};
        
        if (IsPrimitiveNode(node)) {
            float scaledHeight = GetPrimitiveNodeHeight() * canvasState.zoom;
            nodeBounds = {nx, ny, scaledNodeWidth, scaledHeight};
            headerBounds = nodeBounds;
            
            float scaledRadius = pinRadius * canvasState.zoom;
            const Pin* outputPin = nullptr;
            for (const Pin& p : node.outputs) {
                if (p.name == "Value" || p.name == "Component") { outputPin = &p; break; }
            }
            const Pin* targetPin = nullptr;
            for (const Pin& p : node.inputs) {
                if (p.name == "Target") { targetPin = &p; break; }
            }
            bool targetLinked = false;
            if (targetPin) {
                for (const Link& lk : graph.GetLinks()) if (lk.inputPinId == targetPin->id) { targetLinked = true; break; }
            }
            bool showTargetPin = targetPin && (targetLinked || linkFromPin != -1 || CheckCollisionPointRec(mousePos, nodeBounds));

            if (showTargetPin && CheckCollisionPointCircle(mousePos, {nx + scaledRadius + 2, ny + scaledHeight * 0.5f}, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV({nx + scaledRadius + 2, ny + scaledHeight * 0.5f}, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
                    if (targetLinked && IsKeyDown(KEY_LEFT_ALT)) {
                        RemoveLinkFromPin(targetPin->id);
                    } else if (linkFromPin == -1) {
                        linkFromPin = targetPin->id; linkFromIsOutput = false;
                    }
                } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && linkFromPin != -1) {
                    TryCompleteLink(targetPin->id, true);
                }
            }

            if (outputPin && CheckCollisionPointCircle(mousePos, {nx + scaledNodeWidth - scaledRadius - 2, ny + scaledHeight * 0.5f}, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                bool linked = false;
                for (const Link& lk : graph.GetLinks()) if (lk.outputPinId == outputPin->id) { linked = true; break; }
                DrawCircleV({nx + scaledNodeWidth - scaledRadius - 2, ny + scaledHeight * 0.5f}, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
            
            if (showTargetPin) {
                Vector2 pinPos = {nx + scaledRadius + 2, ny + scaledHeight * 0.5f};
                Color pinColor = GetPinColor(targetPin->type);
                if (targetLinked) {
                    DrawCircleV(pinPos, scaledRadius, pinColor);
                } else {
                    DrawCircleV(pinPos, scaledRadius, {40, 40, 42, 255});
                    DrawCircleLinesV(pinPos, scaledRadius, pinColor);
                }
            }

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
            DrawText(displayTitle.c_str(), (int)nx + (targetPin ? 16 : 8), (int)ny + 6, (int)(13 * canvasState.zoom), WHITE);
            
            if (CheckCollisionPointRec(mousePos, nodeBounds) && CheckCollisionPointRec(mousePos, graphArea)) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                    lastValidComponentIndex = -1;
                    draggingNode = true;
                    nodeDragOffset = {mousePos.x - nx, mousePos.y - ny};
                    break;
                }
            }
            
            if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 rawWorld = GraphCanvas::ScreenToWorld(canvasState, graphArea,
                    {mousePos.x - nodeDragOffset.x, mousePos.y - nodeDragOffset.y});
                Vector2 snapped = GraphCanvas::SnapToGrid(rawWorld);
                float snappedX = snapped.x;
                float snappedY = snapped.y;
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
                                Vector2 otherSnapped = GraphCanvas::SnapToGrid({other.posX + deltaX, other.posY + deltaY});
                                other.posX = otherSnapped.x;
                                other.posY = otherSnapped.y;
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
        DrawText(displayTitle.c_str(), (int)nx + 8, (int)ny + 6, (int)(13 * canvasState.zoom), WHITE);

        float inputY = ny + scaledHeaderHeight + scaledPinHeight * 0.5f;
        for (size_t i = 0; i < node.inputs.size(); i++) {
            Pin& pin = node.inputs[i];
            if (pin.name == "Variable_Name") continue;
            if (pin.name == "Component_Name") continue;
            Vector2 pinPos = {nx + pinRadius * canvasState.zoom + 2, inputY};
            Color pinColor = GetPinColor(pin.type);

            bool linked = false;
            for (const Link& lk : graph.GetLinks()) if (lk.inputPinId == pin.id) { linked = true; break; }

            float scaledRadius = pinRadius * canvasState.zoom;

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
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                float fieldX = nx + pinRadius * canvasState.zoom + 18;
                float fieldRightMax = nx + scaledNodeWidth * 0.62f;
                float fieldW = fieldRightMax - fieldX;
                if (fieldW < 40 * canvasState.zoom) fieldW = 40 * canvasState.zoom;
                Rectangle fieldRect = {fieldX, inputY - scaledPinHeight * 0.5f, fieldW, scaledPinHeight - 2};

                if (pin.type == PinType::Boolean) {
                    bool bval = (pin.defaultValue == "true" || pin.defaultValue == "1");
                    if (PropertyField::BoolBox({fieldRect.x, fieldRect.y + 1, fieldRect.height - 2, fieldRect.height - 2}, bval)) {
                        pin.defaultValue = bval ? "true" : "false";
                        isDirty = true;
                    }
                } else if ((node.type == "Input_Key" && pin.name == "Key") ||
                           (node.type == "Override_Input" && pin.name == "New Key")) {
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

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !nodeMenuOpen) {
                        if (CheckCollisionPointRec(mousePos, listenBtn)) {
                            listeningForKeyPinId = pin.id;
                            editingPinId = 0;
                            keyPickerOpen = false;
                            keyPickerPinId = 0;
                        } else if (CheckCollisionPointRec(mousePos, textRect)) {
                            keyPickerKind = PinPickerKind::Key;
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
                } else if (pin.name == "Function Name" && IsTimerFunctionNameNode(node.type)) {
                    if (!keyPickerOpen || keyPickerPinId != pin.id) {
                        DrawRectangleRec(fieldRect, {35, 35, 40, 255});
                        DrawRectangleLinesEx(fieldRect, 1, {60, 60, 70, 255});
                        DrawText(pin.defaultValue.c_str(), (int)fieldRect.x + 6, (int)fieldRect.y + 5, 11, WHITE);
                    }

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::FunctionName;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
                        snprintf(keyPickerSearch, sizeof(keyPickerSearch), "%s", pin.defaultValue.c_str());
                        keyPickerScroll = 0;
                        keyPickerJustOpened = true;
                        keyPickerOpen = true;
                        editingPinId = 0;
                        listeningForKeyPinId = 0;
                    }
                } else if (pin.name == "Component" && pin.type == PinType::Object) {
                    if (!keyPickerOpen || keyPickerPinId != pin.id) {
                        std::string display = pin.defaultValue.empty() ? "Auto" : pin.defaultValue;
                        for (char& c : display) if (c == '_') c = ' ';
                        DrawRectangleRec(fieldRect, {35, 35, 40, 255});
                        DrawRectangleLinesEx(fieldRect, 1, {60, 60, 70, 255});
                        DrawText(display.c_str(), (int)fieldRect.x + 6, (int)fieldRect.y + 5, 11, WHITE);
                    }

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::ComponentRef;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
                        snprintf(keyPickerSearch, sizeof(keyPickerSearch), "%s", pin.defaultValue.c_str());
                        keyPickerScroll = 0;
                        keyPickerJustOpened = true;
                        keyPickerOpen = true;
                        editingPinId = 0;
                        listeningForKeyPinId = 0;
                    }
                } else if (pin.name == "Class" && (node.type == "Cast_To_Class" || node.type == "Get_Actor_Of_Class" || node.type == "For_Each_Actor_Of_Class")) {
                    std::string classPath = pin.defaultValue.c_str();
                    std::string display = "None";
                    if (!classPath.empty()) {
                        size_t slash = classPath.find_last_of("/\\");
                        std::string filename = (slash == std::string::npos) ? classPath : classPath.substr(slash + 1);
                        size_t dot = filename.find_last_of('.');
                        display = (dot == std::string::npos) ? filename : filename.substr(0, dot);
                    }
                    DrawRectangleRec(fieldRect, {35, 35, 40, 255});
                    DrawRectangleLinesEx(fieldRect, 1, {60, 60, 70, 255});
                    DrawText(display.c_str(), (int)fieldRect.x + 6, (int)fieldRect.y + 5, 11, WHITE);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        actorClassPickerPinId = pin.id;
                        actorClassPickerVarIndex = -1;
                        actorClassPicker.OpenWithExtras({fieldRect.x, fieldRect.y + fieldRect.height}, ".staractor",
                                                         BasicActorClassPickerEntries());
                        editingPinId = 0;
                    }
                    if (actorClassPickerPinId == pin.id) {
                        std::string pickedClass;
                        if (actorClassPicker.Render(pickedClass)) {
                            pin.defaultValue = pickedClass;
                            if (!pickedClass.empty()) ObjectKindRegistry::Get().RegisterKind(pickedClass, "Actor");
                            isDirty = true;
                            actorClassPickerPinId = 0;
                        }
                    }
                } else if (node.type == "Override_Input" && pin.name == "Mapping") {
                    if (!keyPickerOpen || keyPickerPinId != pin.id) {
                        std::string display = pin.defaultValue;
                        for (char& c : display) if (c == '_') c = ' ';
                        DrawRectangleRec(fieldRect, {35, 35, 40, 255});
                        DrawRectangleLinesEx(fieldRect, 1, {60, 60, 70, 255});
                        DrawText(display.c_str(), (int)fieldRect.x + 6, (int)fieldRect.y + 5, 11, WHITE);
                    }

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::MappingName;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
                        snprintf(keyPickerSearch, sizeof(keyPickerSearch), "%s", pin.defaultValue.c_str());
                        keyPickerScroll = 0;
                        keyPickerJustOpened = true;
                        keyPickerOpen = true;
                        editingPinId = 0;
                        listeningForKeyPinId = 0;
                    }
                } else {
                    bool allowActivate = (linkFromPin == -1);
                    const char* targetPlaceholder = nullptr;
                    if (pin.name == "Target" && pin.defaultValue.empty()) {
                        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
                        if (tmpl && (tmpl->category == "Actor" || tmpl->category == "Timers")) targetPlaceholder = "self";
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
            Vector2 pinPos = {nx + scaledNodeWidth - pinRadius * canvasState.zoom - 2, outputY};
            Color pinColor = GetPinColor(pin.type);

            bool linked = false;
            for (const Link& lk : graph.GetLinks()) if (lk.outputPinId == pin.id) { linked = true; break; }

            float scaledRadius = pinRadius * canvasState.zoom;

            if (linked) DrawCircleV(pinPos, scaledRadius, pinColor);
            else { DrawCircleV(pinPos, scaledRadius, {40, 40, 42, 255}); DrawCircleLinesV(pinPos, scaledRadius, pinColor); }

            if (pin.name != "Out") {
                float textW = (float)MeasureText(pin.name.c_str(), 12);
                DrawText(pin.name.c_str(), (int)(pinPos.x - textW - 12), (int)outputY - 8, 12, LIGHTGRAY);
            }
            outputY += pinHeight * canvasState.zoom;

if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                lastValidComponentIndex = -1;
                draggingNode = true;
                nodeDragOffset = {mousePos.x - nx, mousePos.y - ny};
                break;
            }
        }

        if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 rawWorld = GraphCanvas::ScreenToWorld(canvasState, graphArea,
                {mousePos.x - nodeDragOffset.x, mousePos.y - nodeDragOffset.y});
            Vector2 snapped = GraphCanvas::SnapToGrid(rawWorld);
            float snappedX = snapped.x;
            float snappedY = snapped.y;
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
                            Vector2 otherSnapped = GraphCanvas::SnapToGrid({other.posX + deltaX, other.posY + deltaY});
                            other.posX = otherSnapped.x;
                            other.posY = otherSnapped.y;
                            break;
                        }
                    }
                }
            }
            isDirty = true;
        }
    }

    if (boxSelecting) {
        Rectangle box = GraphCanvas::NormalizeBox(boxSelectStart, boxSelectEnd);
        DrawRectangleLinesEx(box, 1, {100, 160, 230, 200});
        DrawRectangleRec(box, {100, 160, 230, 30});
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        draggingNode = false;
        if (linkFromPin != -1) {
            bool hitPin = false;
            for (const Node& node : graph.GetNodes()) {
                for (const Pin& pin : node.inputs) {
                    if (CheckCollisionPointCircle(mousePos, GetPinPos(node, pin), pinRadius * canvasState.zoom + 4)) { hitPin = true; break; }
                }
                for (const Pin& pin : node.outputs) {
                    if (CheckCollisionPointCircle(mousePos, GetPinPos(node, pin), pinRadius * canvasState.zoom + 4)) { hitPin = true; break; }
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
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool alt  = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        std::string label = ctrl && alt ? ("Load Saved: " + var.name)
                           : ctrl        ? ("Save: " + var.name)
                           : shift       ? ("Get Saved: " + var.name)
                           : alt         ? ("Set: " + var.name)
                                         : ("Get: " + var.name);
        std::string hint = "  (Alt=Set, Ctrl=Save, Ctrl+Alt=Load Saved, Shift=Get Saved)";
        std::string full = label + hint;
        int w = (int)MeasureText(full.c_str(), 12) + 12;
        DrawRectangleRec({mp.x + 12, mp.y - 4, (float)w, 20}, {60, 60, 65, 255});
        DrawRectangleLinesEx({mp.x + 12, mp.y - 4, (float)w, 20}, 1, {100, 100, 110, 255});
        DrawText(label.c_str(), (int)(mp.x + 18), (int)(mp.y), 12, WHITE);
        DrawText(hint.c_str(), (int)(mp.x + 18 + MeasureText(label.c_str(), 12)), (int)(mp.y), 12, {130, 130, 140, 255});

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = nullptr;
                const Variable& var = graph.GetVariables()[variableDragIndex];

                if (ctrl && alt) {
                    node = graph.CreateNode("Load_Saved_Variable");
                    node->title = "Load Saved: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                    }
                    for (Pin& p : node->outputs) {
                        if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; }
                    }
                } else if (ctrl) {
                    node = graph.CreateNode("Save_Variable");
                    node->title = "Save: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                    }
                } else if (shift) {
                    node = graph.CreateNode("Get_Saved_Variable");
                    node->title = "Get Saved: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                    }
                    for (Pin& p : node->outputs) {
                        if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; }
                    }
                } else if (IsKeyDown(KEY_LEFT_ALT)) {
                    node = graph.CreateNode("Set_Variable");
                    node->title = "Set: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                        if (p.name == "Value")         { p.type = var.type; p.objectKind = var.objectKind; }
                    }
                } else {
                    node = graph.CreateNode("Get_Variable");
                    node->title = "Get: " + var.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") p.defaultValue = var.name;
                    }
                    for (Pin& p : node->outputs) {
                        if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; }
                    }
                }

                Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, mp));
                node->posX = snapped.x;
                node->posY = snapped.y;
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
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = graph.CreateNode("Get_Component");
                node->title = "Get: " + comp.name;
                node->componentClass = comp.componentType;
                for (Pin& p : node->inputs) {
                    if (p.name == "Component_Name") p.defaultValue = comp.name;
                }
                Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, mp));
                node->posX = snapped.x;
                node->posY = snapped.y;
                isDirty = true;
            }
            componentDragActive = false;
            componentDragIndex = -1;
        }
    } else if (componentDragIndex >= 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        componentDragActive = false;
        componentDragIndex = -1;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !draggingNode && editingPinId == 0 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
            Rectangle sel = GraphCanvas::NormalizeBox(boxSelectStart, boxSelectEnd);
            if (sel.width > 4 || sel.height > 4) {
                if (!IsKeyDown(KEY_LEFT_CONTROL)) selectedNodeIds.clear();
                for (const Node& node : graph.GetNodes()) {
                    float nx2 = graphArea.x + node.posX * canvasState.zoom + canvasState.scroll.x;
                    float ny2 = graphArea.y + node.posY * canvasState.zoom + canvasState.scroll.y;
                    float nw2 = GetNodeWidth(node) * canvasState.zoom;
                    float nh2 = GetNodeHeight(node) * canvasState.zoom;
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

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
        if (!canvasState.didPan) {
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
    while (!searchLower.empty() && searchLower.back() == ' ') searchLower.pop_back();
    size_t searchStart = searchLower.find_first_not_of(' ');
    searchLower = (searchStart == std::string::npos) ? "" : searchLower.substr(searchStart);

    std::vector<std::pair<std::string, std::string>> entries;

    const Pin* dragFromPin = nullptr;
    bool dragFromIsOutput = false;
    Node* dragSourceNode = nullptr;
    std::string sourceKind;
    if (filterCompatibleWithPin != -1) {
        for (Node& node : const_cast<std::deque<Node>&>(graph.GetNodes())) {
            for (Pin& pin : node.outputs) if (pin.id == filterCompatibleWithPin) { dragFromPin = &pin; dragFromIsOutput = true; dragSourceNode = &node; break; }
            for (Pin& pin : node.inputs)  if (pin.id == filterCompatibleWithPin) { dragFromPin = &pin; dragFromIsOutput = false; dragSourceNode = &node; break; }
        }
        if (dragFromPin) sourceKind = EffectivePinObjectKind(dragSourceNode, *dragFromPin);
    }

    for (const auto& t : templates) {
        if (t.type == "Get_Variable" || t.type == "Set_Variable") continue;
        if (t.type == "Call_Custom_Event") continue;
        if (t.type == "Input_Mapping") continue;

        if (!searchLower.empty()) {
            std::string typeLower = NodeMenuDisplayLabel(t.type);
            for (char& c : typeLower) c = (char)tolower(c);
            if (typeLower.find(searchLower) == std::string::npos) continue;
        }

        if (filterCompatibleWithPin != -1) {
            const Pin* fromPin = dragFromPin;
            bool fromIsOutput = dragFromIsOutput;
            if (fromPin) {
                bool hasCompatible = false;
                if (fromIsOutput) {
                    for (const Pin& p : t.createPins ? [&]() { Node tmp; t.createPins(&tmp); return tmp.inputs; }() : std::vector<Pin>{}) {
                        if (p.type == PinType::Wildcard) { hasCompatible = true; break; }
                        if (p.type != fromPin->type) continue;
                        if (p.type == PinType::Object) {
                            if (ObjectKindRegistry::Get().IsCompatible(sourceKind, p.objectKind)) { hasCompatible = true; break; }
                        } else {
                            hasCompatible = true; break;
                        }
                    }
                } else {
                    for (const Pin& p : t.createPins ? [&]() { Node tmp; t.createPins(&tmp); return tmp.outputs; }() : std::vector<Pin>{}) {
                        if (p.type == PinType::Wildcard) { hasCompatible = true; break; }
                        if (p.type != fromPin->type) continue;
                        if (p.type == PinType::Object) {
                            if (ObjectKindRegistry::Get().IsCompatible(p.objectKind, sourceKind)) { hasCompatible = true; break; }
                        } else {
                            hasCompatible = true; break;
                        }
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

        std::string typeLower = NodeMenuDisplayLabel("Call_Custom_Event:" + eventName);
        for (char& c : typeLower) c = (char)tolower(c);
        if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

        entries.push_back({"Events", "Call_Custom_Event:" + eventName});
    }

    for (const ComponentDecl& comp : graph.GetComponents()) {
        if (comp.componentType != "InputComponent") continue;
        for (const std::string& mappingName : comp.inputMappingNames) {
            if (mappingName.empty()) continue;

            std::string typeLower = NodeMenuDisplayLabel("Input_Mapping:" + mappingName);
            for (char& c : typeLower) c = (char)tolower(c);
            if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

            std::string nodeId = "Input_Mapping:" + mappingName;
            bool alreadyListed = false;
            for (const auto& e : entries) if (e.second == nodeId) { alreadyListed = true; break; }
            if (alreadyListed) continue;

            entries.push_back({"Input", nodeId});
        }
    }

    if (!sourceKind.empty() && sourceKind.find('.') != std::string::npos) {
        if (const Stargraph* externalGraph = StargraphAssetCache::Get().GetOrLoad(sourceKind)) {
            for (const Node& evNode : externalGraph->GetNodes()) {
                if (evNode.type != "Custom_Event") continue;
                std::string eventName = evNode.title;
                if (eventName.empty()) continue;

                std::string nodeId = "Call_Custom_Event:" + eventName;
                std::string typeLower = NodeMenuDisplayLabel(nodeId);
                for (char& c : typeLower) c = (char)tolower(c);
                if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

                bool alreadyListed = false;
                for (const auto& e : entries) if (e.second == nodeId) { alreadyListed = true; break; }
                if (!alreadyListed) entries.push_back({"Events", nodeId});
            }

            for (const Variable& var : externalGraph->GetVariables()) {
                if (var.name.empty()) continue;

                std::string getId = "Get_External_Var:" + var.name;
                std::string getLower = NodeMenuDisplayLabel(getId);
                for (char& c : getLower) c = (char)tolower(c);
                if (searchLower.empty() || getLower.find(searchLower) != std::string::npos)
                    entries.push_back({"Variables", getId});

                std::string setId = "Set_External_Var:" + var.name;
                std::string setLower = NodeMenuDisplayLabel(setId);
                for (char& c : setLower) c = (char)tolower(c);
                if (searchLower.empty() || setLower.find(searchLower) != std::string::npos)
                    entries.push_back({"Variables", setId});
            }

            for (const ComponentDecl& comp : externalGraph->GetComponents()) {
                if (comp.name.empty()) continue;

                std::string compId = "Get_External_Component:" + comp.name;
                std::string compLower = NodeMenuDisplayLabel(compId);
                for (char& c : compLower) c = (char)tolower(c);
                if (searchLower.empty() || compLower.find(searchLower) != std::string::npos)
                    entries.push_back({"Components", compId});
            }
        }
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

    DrawRectangleRec(popup, UITheme::Get().panelBg);
    DrawRectangleLinesEx(popup, 1, UITheme::Get().panelBorder);

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
    const UIPalette& theme = UITheme::Get();
    for (const auto& entry : entries) {
        if (entry.first != lastCategory) {
            if (!lastCategory.empty()) {
                DrawLineV({popup.x + 6, y}, {popup.x + popup.width - 6, y}, theme.panelBorder);
            }
            lastCategory = entry.first;
            DrawText(entry.first.c_str(), (int)popup.x + 8, (int)y + 4, 11, theme.accent);
            y += categoryHeaderH;
        }

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        if (CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea)) {
            DrawRectangleRec(btn, theme.surfaceHover);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                std::string nodeType = entry.second;
                std::string eventName;
                std::string mappingName;
                std::string externalVarName;
                std::string externalCompName;
                bool isExternalSet = false;
                if (nodeType.rfind("Call_Custom_Event:", 0) == 0) {
                    eventName = nodeType.substr(18);
                    nodeType = "Call_Custom_Event";
                } else if (nodeType.rfind("Input_Mapping:", 0) == 0) {
                    mappingName = nodeType.substr(14);
                    nodeType = "Input_Mapping";
                } else if (nodeType.rfind("Get_External_Var:", 0) == 0) {
                    externalVarName = nodeType.substr(17);
                    nodeType = "Get_Variable";
                } else if (nodeType.rfind("Set_External_Var:", 0) == 0) {
                    externalVarName = nodeType.substr(17);
                    nodeType = "Set_Variable";
                    isExternalSet = true;
                } else if (nodeType.rfind("Get_External_Component:", 0) == 0) {
                    externalCompName = nodeType.substr(23);
                    nodeType = "Get_Component";
                }
                Node* node = graph.CreateNode(nodeType);
                if (!mappingName.empty()) {
                    for (Pin& p : node->inputs) {
                        if (p.name == "Mapping") { p.defaultValue = mappingName; break; }
                    }
                }
                if (!eventName.empty()) {
                    for (Pin& p : node->inputs) {
                        if (p.name == "Event Name") {
                            p.defaultValue = eventName;
                            break;
                        }
                    }
                    Node* targetEvent = graph.FindCustomEventByName(eventName);
                    if (!targetEvent && !sourceKind.empty()) {
                        if (const Stargraph* ext = StargraphAssetCache::Get().GetOrLoad(sourceKind)) {
                            targetEvent = const_cast<Stargraph*>(ext)->FindCustomEventByName(eventName);
                        }
                    }
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
                if (!externalVarName.empty()) {
                    for (Pin& p : node->inputs) {
                        if (p.name == "Variable_Name") { p.defaultValue = externalVarName; break; }
                    }
                    if (const Stargraph* ext = StargraphAssetCache::Get().GetOrLoad(sourceKind)) {
                        for (const Variable& var : ext->GetVariables()) {
                            if (var.name != externalVarName) continue;
                            if (isExternalSet) {
                                for (Pin& p : node->inputs) if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; break; }
                            } else {
                                for (Pin& p : node->outputs) if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; break; }
                            }
                            break;
                        }
                    }
                    node->title = (isExternalSet ? "Set: " : "Get: ") + externalVarName;
                }
                if (!externalCompName.empty()) {
                    for (Pin& p : node->inputs) {
                        if (p.name == "Component_Name") { p.defaultValue = externalCompName; break; }
                    }
                    if (const Stargraph* ext = StargraphAssetCache::Get().GetOrLoad(sourceKind)) {
                        for (const ComponentDecl& comp : ext->GetComponents()) {
                            if (comp.name != externalCompName) continue;
                            node->componentClass = comp.componentType;
                            break;
                        }
                    }
                    node->title = "Get: " + externalCompName;
                }
                Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, nodeMenuPos));
                node->posX = snapped.x;
                node->posY = snapped.y;

                if (filterCompatibleWithPin != -1) {
                    const Pin* fromPin = nullptr;
                    bool fromIsOutput = false;
                    const Node* fromNode = nullptr;
                    for (const Node& n : graph.GetNodes()) {
                        for (const Pin& pin : n.outputs) if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = true; fromNode = &n; }
                        for (const Pin& pin : n.inputs)  if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = false; fromNode = &n; }
                    }
                    if (fromPin) {
                        std::string sourceKind = EffectivePinObjectKind(fromNode, *fromPin);
                        if (fromIsOutput) {
                            for (const Pin& p : node->inputs) {
                                if (p.type == PinType::Wildcard) { graph.AddLink({filterCompatibleWithPin, p.id}); break; }
                                if (p.type != fromPin->type) continue;
                                if (p.type == PinType::Object) {
                                    if (!ObjectKindRegistry::Get().IsCompatible(sourceKind, p.objectKind)) continue;
                                }
                                graph.AddLink({filterCompatibleWithPin, p.id});
                                break;
                            }
                        } else {
                            for (const Pin& p : node->outputs) {
                                if (p.type == PinType::Wildcard) { graph.AddLink({p.id, filterCompatibleWithPin}); break; }
                                if (p.type != fromPin->type) continue;
                                if (p.type == PinType::Object) {
                                    if (!ObjectKindRegistry::Get().IsCompatible(p.objectKind, sourceKind)) continue;
                                }
                                graph.AddLink({p.id, filterCompatibleWithPin});
                                break;
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
        std::string entryDisplay = NodeMenuDisplayLabel(entry.second);
        DrawText(entryDisplay.c_str(), (int)btn.x + 8, (int)btn.y + 4, 12, theme.textPrimary);
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