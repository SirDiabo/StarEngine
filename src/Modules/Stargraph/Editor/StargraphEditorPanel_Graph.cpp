#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/ObjectEvents.h"
#include "Core/InputKeyNames.h"
#include "Modules/Stargraph/StargraphAssetCache.h"
#include "Core/ObjectKindRegistry.h"
#include "Core/SafeParse.h"
#include "Editor/UITheme.h"
#include "Editor/BasicActorDefs.h"
#include "Core/Widgets/DragDropController.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
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

static const std::string kBoundElementEventPrefix = "Bound_Element_Event:";

static std::string NodeMenuDisplayLabel(const std::string& rawType) {
    if (rawType.rfind(kBoundElementEventPrefix, 0) == 0) {
        std::string rest = rawType.substr(kBoundElementEventPrefix.size());
        size_t separator = rest.find(':');
        if (separator != std::string::npos) {
            std::string classLabel = "Element";
            if (const NodeTemplate* boundTemplate = NodeRegistry::Get().GetTemplate(rest.substr(0, separator))) {
                classLabel = boundTemplate->boundElementClass;
                const std::string suffix = "Element";
                if (classLabel.size() > suffix.size() &&
                    classLabel.compare(classLabel.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    classLabel.resize(classLabel.size() - suffix.size());
                }
            }
            return classLabel + " Event " + rest.substr(separator + 1);
        }
    }
    if (const NodeTemplate* labelTemplate = NodeRegistry::Get().GetTemplate(rawType)) {
        if (!labelTemplate->menuLabel.empty()) return labelTemplate->menuLabel;
    }
    std::string display = rawType;
    if (display.rfind("Call_Custom_Event:", 0) == 0) {
        display = "Event " + display.substr(18);
    } else if (display.rfind("Call_Function:", 0) == 0) {
        display = "Function " + display.substr(14);
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
    if (pin.isOutput && !node->componentClass.empty()) return node->componentClass;
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
        case PinType::Rotator: return {172,  89, 227, 255};
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
            for (const Link& lk : ActiveGraph().GetLinks()) {
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
            for (const Link& lk : ActiveGraph().GetLinks()) {
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
    for (const Node& node : ActiveGraph().GetNodes()) {
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

static const char* StringConversionNodeType(PinType from) {
    switch (from) {
        case PinType::Float:   return "Float_To_String";
        case PinType::Integer: return "Int_To_String";
        case PinType::Boolean: return "Bool_To_String";
        case PinType::Vec3:    return "Vec3_To_String";
        case PinType::Rotator: return "Rotator_To_String";
        default: return nullptr;
    }
}

bool StargraphEditorPanel::ConnectPins(int outputPinId, int inputPinId) {
    for (const Link& lk : ActiveGraph().GetLinks()) {
        if (lk.inputPinId == inputPinId) return false;
    }

    if (PinsCompatible(outputPinId, inputPinId)) {
        ActiveGraph().AddLink({outputPinId, inputPinId});
        RefreshWildcardPins(ActiveGraph().FindNodeByPinId(outputPinId));
        RefreshWildcardPins(ActiveGraph().FindNodeByPinId(inputPinId));
        return true;
    }

    Pin* outPin = ActiveGraph().FindPin(outputPinId);
    Pin* inPin = ActiveGraph().FindPin(inputPinId);
    Node* inNode = ActiveGraph().FindNodeByPinId(inputPinId);
    if (!outPin || !inPin || !inNode || inPin->type != PinType::String) return false;

    const char* conversionType = StringConversionNodeType(outPin->type);
    if (!conversionType || !NodeRegistry::Get().GetTemplate(conversionType)) return false;

    Vector2 inputPinWorld = GraphCanvas::ScreenToWorld(canvasState, graphArea, GetPinPos(*inNode, *inPin));

    Node* conversion = ActiveGraph().CreateNode(conversionType);
    if (!conversion) return false;

    int valuePinId = -1, resultPinId = -1;
    for (const Pin& p : conversion->inputs) if (p.name == "Value") { valuePinId = p.id; break; }
    for (const Pin& p : conversion->outputs) if (p.name == "Result") { resultPinId = p.id; break; }
    if (valuePinId == -1 || resultPinId == -1) {
        ActiveGraph().RemoveNode(std::stoi(conversion->id));
        return false;
    }

    Vector2 position = GraphCanvas::SnapToGrid({
        inNode->posX - GetNodeWidth(*conversion) - 48.0f,
        inputPinWorld.y - headerHeight - pinHeight * 0.5f
    });
    conversion->posX = position.x;
    conversion->posY = position.y;

    ActiveGraph().AddLink({outputPinId, valuePinId});
    ActiveGraph().AddLink({resultPinId, inputPinId});
    RefreshWildcardPins(ActiveGraph().FindNodeByPinId(outputPinId));
    return true;
}

void StargraphEditorPanel::TryCompleteLink(int pinId, bool onInput) {
    if (linkFromPin == -1) return;

    int outputPinId = linkFromIsOutput ? linkFromPin : pinId;
    int inputPinId  = linkFromIsOutput ? pinId       : linkFromPin;

    if (!ConnectPins(outputPinId, inputPinId)) { CancelLink(); return; }
    isDirty = true;

    for (int extraPinId : rerouteExtraPins) {
        int extraOutputPinId = linkFromIsOutput ? extraPinId : pinId;
        int extraInputPinId  = linkFromIsOutput ? pinId       : extraPinId;
        ConnectPins(extraOutputPinId, extraInputPinId);
    }

    CancelLink();
}

void StargraphEditorPanel::CloseNodeMenu() {
    nodeMenuOpen = false;
    filterCompatibleWithPin = -1;
    if (nodeMenuSearchField && nodeMenuSearchField->isEditing) nodeMenuSearchField->OnDeselected();
}

void StargraphEditorPanel::CancelLink() {
    linkFromPin = -1;
    linkFromIsOutput = false;
    rerouteExtraPins.clear();
}

void StargraphEditorPanel::RemoveLinkFromPin(int pinId) {
    int otherPinId = -1;
    for (const Link& lk : ActiveGraph().GetLinks()) {
        if (lk.inputPinId == pinId)  { otherPinId = lk.outputPinId; break; }
        if (lk.outputPinId == pinId) { otherPinId = lk.inputPinId;  break; }
    }
    ActiveGraph().RemoveLink(pinId);
    isDirty = true;
    RefreshWildcardPins(ActiveGraph().FindNodeByPinId(pinId));
    if (otherPinId != -1) RefreshWildcardPins(ActiveGraph().FindNodeByPinId(otherPinId));
}

void StargraphEditorPanel::RemoveSpecificLink(int outputPinId, int inputPinId) {
    ActiveGraph().RemoveSpecificLink(outputPinId, inputPinId);
    isDirty = true;
    RefreshWildcardPins(ActiveGraph().FindNodeByPinId(outputPinId));
    RefreshWildcardPins(ActiveGraph().FindNodeByPinId(inputPinId));
}

void StargraphEditorPanel::BeginReroute(int pinId) {
    std::vector<int> otherPinIds;
    for (const Link& lk : ActiveGraph().GetLinks()) {
        if (lk.outputPinId == pinId) otherPinIds.push_back(lk.inputPinId);
        else if (lk.inputPinId == pinId) otherPinIds.push_back(lk.outputPinId);
    }
    if (otherPinIds.empty()) return;

    RemoveLinkFromPin(pinId);

    linkFromPin = otherPinIds[0];
    linkFromIsOutput = false;
    for (const Node& n : ActiveGraph().GetNodes()) {
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
        for (const Link& lk : ActiveGraph().GetLinks()) {
            if (lk.inputPinId != pin.id) continue;
            if (const Node* fromNode = ActiveGraph().FindNodeByPinId(lk.outputPinId)) {
                for (const Pin& p : fromNode->outputs) {
                    if (p.id == lk.outputPinId && p.type != PinType::Wildcard) resolved = p.type;
                }
            }
        }
    }
    if (resolved == PinType::None) {
        for (const Pin& pin : node->outputs) {
            if (pin.type != PinType::Wildcard) continue;
            for (const Link& lk : ActiveGraph().GetLinks()) {
                if (lk.outputPinId != pin.id) continue;
                if (const Node* toNode = ActiveGraph().FindNodeByPinId(lk.inputPinId)) {
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

namespace {

struct ClipPin {
    std::string name;
    bool isInput = true;
    bool hasType = false;
    PinType type = PinType::None;
    std::string kind;
    bool hasValue = false;
    std::string value;
};

struct ClipNode {
    std::string type;
    std::string title;
    float posX = 0, posY = 0;
    std::string componentClass;
    ReplicationMode replication = ReplicationMode::Local;
    bool reliable = false;
    std::vector<ClipPin> pins;
};

struct ClipLink {
    int fromNode = -1;
    std::string fromPin;
    int toNode = -1;
    std::string toPin;
};

struct PinLocation {
    const Node* node = nullptr;
    const Pin* pin = nullptr;
};

std::string EscapeClipValue(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

std::string UnescapeClipValue(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char next = text[++i];
            if (next == 'n') out += '\n';
            else if (next == 'r') out += '\r';
            else out += next;
        } else {
            out += text[i];
        }
    }
    return out;
}

std::string ClipPinTypeToString(PinType type) {
    return type == PinType::Wildcard ? "Wildcard" : PinTypeToString(type);
}

PinType ClipPinTypeFromString(const std::string& text) {
    return text == "Wildcard" ? PinType::Wildcard : PinTypeFromString(text);
}

PinLocation LocatePin(const Stargraph& graph, int pinId, bool isOutput) {
    for (const Node& node : graph.GetNodes()) {
        const std::vector<Pin>& pins = isOutput ? node.outputs : node.inputs;
        for (const Pin& pin : pins) {
            if (pin.id == pinId) return {&node, &pin};
        }
    }
    return {};
}

ClipNode CaptureNode(const Node& node) {
    ClipNode clip;
    clip.type = node.type;
    clip.title = node.title;
    clip.posX = node.posX;
    clip.posY = node.posY;
    clip.componentClass = node.componentClass;
    clip.replication = node.eventReplication;
    clip.reliable = node.eventReliable;
    for (int side = 0; side < 2; side++) {
        const bool isInput = (side == 0);
        for (const Pin& pin : (isInput ? node.inputs : node.outputs)) {
            if (pin.type == PinType::Exec) continue;
            ClipPin cp;
            cp.name = pin.name;
            cp.isInput = isInput;
            cp.hasType = true;
            cp.type = pin.type;
            cp.kind = pin.objectKind;
            if (isInput) {
                cp.hasValue = true;
                cp.value = (const std::string&)pin.defaultValue;
            }
            clip.pins.push_back(cp);
        }
    }
    return clip;
}

Node* InstantiateNode(Stargraph& graph, const ClipNode& clip) {
    Node* node = graph.CreateNode(clip.type);
    if (!node) return nullptr;
    node->title = clip.title;
    node->componentClass = clip.componentClass;
    node->eventReplication = clip.replication;
    node->eventReliable = clip.reliable;

    for (const ClipPin& cp : clip.pins) {
        std::vector<Pin>& pins = cp.isInput ? node->inputs : node->outputs;
        Pin* target = nullptr;
        for (Pin& pin : pins) {
            if (pin.name == cp.name && pin.type != PinType::Exec) { target = &pin; break; }
        }
        if (!target) target = graph.AddDynamicPin(*node, !cp.isInput, cp.name, cp.hasType ? cp.type : PinType::None);
        if (!target) continue;
        if (cp.hasType && cp.type != PinType::Wildcard) target->type = cp.type;
        target->objectKind = cp.kind;
        if (cp.isInput && cp.hasValue) target->defaultValue = cp.value;
    }

    for (int side = 0; side < 2; side++) {
        const bool isInput = (side == 0);
        std::vector<std::string> stale;
        for (const Pin& pin : (isInput ? node->inputs : node->outputs)) {
            if (pin.type == PinType::Exec) continue;
            bool present = false;
            for (const ClipPin& cp : clip.pins) {
                if (cp.isInput == isInput && cp.name == pin.name) { present = true; break; }
            }
            if (!present) stale.push_back(pin.name);
        }
        for (const std::string& name : stale) graph.RemoveDynamicPin(*node, !isInput, name);
    }
    return node;
}

std::vector<ClipLink> CollectInternalLinks(const Stargraph& graph, const std::vector<const Node*>& nodes) {
    std::vector<ClipLink> result;
    auto indexOf = [&](const Node* node) {
        for (size_t i = 0; i < nodes.size(); i++) if (nodes[i] == node) return (int)i;
        return -1;
    };
    for (const Link& link : graph.GetLinks()) {
        PinLocation from = LocatePin(graph, link.outputPinId, true);
        PinLocation to = LocatePin(graph, link.inputPinId, false);
        if (!from.node || !to.node) continue;
        int fromIdx = indexOf(from.node);
        int toIdx = indexOf(to.node);
        if (fromIdx < 0 || toIdx < 0) continue;
        ClipLink cl;
        cl.fromNode = fromIdx;
        cl.fromPin = from.pin->name;
        cl.toNode = toIdx;
        cl.toPin = to.pin->name;
        result.push_back(cl);
    }
    return result;
}

void LinkClonedNodes(Stargraph& graph, const std::vector<Node*>& created, const std::vector<ClipLink>& links) {
    for (const ClipLink& cl : links) {
        if (cl.fromNode < 0 || cl.fromNode >= (int)created.size()) continue;
        if (cl.toNode < 0 || cl.toNode >= (int)created.size()) continue;
        Node* fromNode = created[cl.fromNode];
        Node* toNode = created[cl.toNode];
        if (!fromNode || !toNode) continue;
        int outPinId = -1, inPinId = -1;
        for (const Pin& pin : fromNode->outputs) if (pin.name == cl.fromPin) { outPinId = pin.id; break; }
        for (const Pin& pin : toNode->inputs) if (pin.name == cl.toPin) { inPinId = pin.id; break; }
        if (outPinId != -1 && inPinId != -1) graph.AddLink({outPinId, inPinId});
    }
}

std::string SerializeClipNode(const ClipNode& clip, float originX, float originY) {
    std::string text = "[Node]\n";
    text += "Type=" + clip.type + "\n";
    text += "Title=" + EscapeClipValue(clip.title) + "\n";
    text += "PosX=" + std::to_string(clip.posX - originX) + "\n";
    text += "PosY=" + std::to_string(clip.posY - originY) + "\n";
    if (!clip.componentClass.empty()) text += "ComponentClass=" + clip.componentClass + "\n";
    if (clip.replication != ReplicationMode::Local) text += "Replication=" + ReplicationModeToString(clip.replication) + "\n";
    if (clip.reliable) text += "Reliable=true\n";
    for (const ClipPin& cp : clip.pins) {
        if (cp.isInput) {
            text += "Pin." + cp.name + "=" + EscapeClipValue(cp.value) + "\n";
            text += "PinType." + cp.name + "=" + ClipPinTypeToString(cp.type) + "\n";
            if (!cp.kind.empty()) text += "PinKind." + cp.name + "=" + cp.kind + "\n";
        } else {
            text += "OutPinType." + cp.name + "=" + ClipPinTypeToString(cp.type) + "\n";
            if (!cp.kind.empty()) text += "OutPinKind." + cp.name + "=" + cp.kind + "\n";
        }
    }
    text += "\n";
    return text;
}

std::string SerializeClipLink(const ClipLink& link) {
    std::string text = "[Link]\n";
    text += "FromNode=" + std::to_string(link.fromNode) + "\n";
    text += "FromPin=" + link.fromPin + "\n";
    text += "ToNode=" + std::to_string(link.toNode) + "\n";
    text += "ToPin=" + link.toPin + "\n";
    text += "\n";
    return text;
}

void ParseClipboardText(const std::string& text, std::vector<ClipNode>& nodes, std::vector<ClipLink>& links) {
    enum class Section { None, InNode, InLink };
    Section section = Section::None;
    ClipNode currentNode;
    ClipLink currentLink;

    auto flush = [&]() {
        if (section == Section::InNode) nodes.push_back(currentNode);
        else if (section == Section::InLink && currentLink.fromNode >= 0 && currentLink.toNode >= 0) links.push_back(currentLink);
        currentNode = ClipNode();
        currentLink = ClipLink();
        section = Section::None;
    };

    auto pinSlot = [&](bool isInput, const std::string& name) -> ClipPin& {
        for (ClipPin& cp : currentNode.pins) if (cp.isInput == isInput && cp.name == name) return cp;
        ClipPin cp;
        cp.name = name;
        cp.isInput = isInput;
        currentNode.pins.push_back(cp);
        return currentNode.pins.back();
    };

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) { flush(); continue; }
        if (line == "[Node]") { flush(); section = Section::InNode; continue; }
        if (line == "[Link]") { flush(); section = Section::InLink; continue; }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        if (section == Section::InNode) {
            if (key == "Type") currentNode.type = value;
            else if (key == "Title") currentNode.title = UnescapeClipValue(value);
            else if (key == "PosX") currentNode.posX = SafeParse::ToFloat(value);
            else if (key == "PosY") currentNode.posY = SafeParse::ToFloat(value);
            else if (key == "ComponentClass") currentNode.componentClass = value;
            else if (key == "Replication") currentNode.replication = ReplicationModeFromString(value);
            else if (key == "Reliable") currentNode.reliable = (value == "true");
            else if (key.rfind("Pin.", 0) == 0) { ClipPin& cp = pinSlot(true, key.substr(4)); cp.hasValue = true; cp.value = UnescapeClipValue(value); }
            else if (key.rfind("PinType.", 0) == 0) { ClipPin& cp = pinSlot(true, key.substr(8)); cp.hasType = true; cp.type = ClipPinTypeFromString(value); }
            else if (key.rfind("PinKind.", 0) == 0) pinSlot(true, key.substr(8)).kind = value;
            else if (key.rfind("OutPinType.", 0) == 0) { ClipPin& cp = pinSlot(false, key.substr(11)); cp.hasType = true; cp.type = ClipPinTypeFromString(value); }
            else if (key.rfind("OutPinKind.", 0) == 0) pinSlot(false, key.substr(11)).kind = value;
        } else if (section == Section::InLink) {
            if (key == "FromNode") currentLink.fromNode = SafeParse::ToInt(value, -1);
            else if (key == "FromPin") currentLink.fromPin = value;
            else if (key == "ToNode") currentLink.toNode = SafeParse::ToInt(value, -1);
            else if (key == "ToPin") currentLink.toPin = value;
        }
    }
    flush();
}

}

void StargraphEditorPanel::ResolveClonedWildcards(const std::vector<Node*>& nodes) {
    auto snapshot = [](const Node& node) {
        std::vector<PinType> types;
        for (const Pin& pin : node.inputs) types.push_back(pin.type);
        for (const Pin& pin : node.outputs) types.push_back(pin.type);
        return types;
    };

    bool changed = true;
    for (size_t pass = 0; changed && pass <= nodes.size(); pass++) {
        changed = false;
        for (Node* node : nodes) {
            if (!node) continue;
            std::vector<PinType> before = snapshot(*node);
            RefreshWildcardPins(node);
            if (before != snapshot(*node)) changed = true;
        }
    }
}

void StargraphEditorPanel::CopySelectedNodes() {
    std::vector<int> nodesToCopy = selectedNodeIds;
    if (selectedNodeId != -1 && std::find(selectedNodeIds.begin(), selectedNodeIds.end(), selectedNodeId) == selectedNodeIds.end()) {
        nodesToCopy.push_back(selectedNodeId);
    }

    std::vector<const Node*> sources;
    for (int nodeId : nodesToCopy) {
        const Node* node = ActiveGraph().FindNodeById(nodeId);
        if (node) sources.push_back(node);
    }
    if (sources.empty()) return;

    float minX = FLT_MAX, minY = FLT_MAX;
    for (const Node* node : sources) {
        if (node->posX < minX) minX = node->posX;
        if (node->posY < minY) minY = node->posY;
    }

    std::string clipText = "[StargraphCopy]\n";
    for (const Node* node : sources) clipText += SerializeClipNode(CaptureNode(*node), minX, minY);
    for (const ClipLink& link : CollectInternalLinks(ActiveGraph(), sources)) clipText += SerializeClipLink(link);

    SetClipboardText(clipText.c_str());
}

void StargraphEditorPanel::PasteNodes() {
    const char* clipText = GetClipboardText();
    if (!clipText || clipText[0] == '\0') return;

    std::string clipboard = clipText;
    if (clipboard.find("[StargraphCopy]\n") == std::string::npos) return;

    std::vector<ClipNode> parsedNodes;
    std::vector<ClipLink> parsedLinks;
    ParseClipboardText(clipboard, parsedNodes, parsedLinks);
    if (parsedNodes.empty()) return;

    Vector2 rawWorld = GraphCanvas::ScreenToWorld(canvasState, graphArea, GetMousePosition());
    const float gridSnap = 16.0f;

    std::vector<Node*> createdNodes;
    for (const ClipNode& clip : parsedNodes) {
        Node* node = InstantiateNode(ActiveGraph(), clip);
        if (node) {
            node->posX = roundf((rawWorld.x + clip.posX) / gridSnap) * gridSnap;
            node->posY = roundf((rawWorld.y + clip.posY) / gridSnap) * gridSnap;
        }
        createdNodes.push_back(node);
    }

    LinkClonedNodes(ActiveGraph(), createdNodes, parsedLinks);
    ResolveClonedWildcards(createdNodes);

    isDirty = true;
}

void StargraphEditorPanel::DuplicateSelected() {
    std::vector<int> nodesToDup = selectedNodeIds;
    if (selectedNodeId != -1 && std::find(selectedNodeIds.begin(), selectedNodeIds.end(), selectedNodeId) == selectedNodeIds.end()) {
        nodesToDup.push_back(selectedNodeId);
    }
    if (nodesToDup.empty()) return;

    const float offset = 32.0f;
    std::vector<const Node*> sources;
    for (int nodeId : nodesToDup) {
        const Node* src = ActiveGraph().FindNodeById(nodeId);
        if (src) sources.push_back(src);
    }
    if (sources.empty()) return;

    std::vector<ClipLink> internalLinks = CollectInternalLinks(ActiveGraph(), sources);

    std::vector<Node*> newNodes;
    for (const Node* src : sources) {
        Node* node = InstantiateNode(ActiveGraph(), CaptureNode(*src));
        if (node) {
            node->posX = src->posX + offset;
            node->posY = src->posY + offset;
        }
        newNodes.push_back(node);
    }

    LinkClonedNodes(ActiveGraph(), newNodes, internalLinks);
    ResolveClonedWildcards(newNodes);

    selectedNodeIds.clear();
    for (Node* n : newNodes) if (n) selectedNodeIds.push_back(std::stoi(n->id));
    selectedNodeId = selectedNodeIds.empty() ? -1 : selectedNodeIds.back();

    isDirty = true;
}

void StargraphEditorPanel::DeleteSelected() {
    if (selectedNodeId == -1 && selectedNodeIds.empty()) return;

    if (!selectedNodeIds.empty()) {
        for (int id : selectedNodeIds) {
            Node* n = ActiveGraph().FindNodeById(id);
            if (n && n->type == "Function_Entry") continue;
            ActiveGraph().RemoveNode(id);
        }
        selectedNodeIds.clear();
        selectedNodeId = -1;
    } else {
        Node* n = ActiveGraph().FindNodeById(selectedNodeId);
        if (!(n && n->type == "Function_Entry")) {
            ActiveGraph().RemoveNode(selectedNodeId);
        }
        selectedNodeId = -1;
    }
    isDirty = true;
}

Node* StargraphEditorPanel::GetNodeAtPos(Vector2 pos) {
    for (Node& node : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
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

        for (const Node& node : ActiveGraph().GetNodes()) {
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
    const UIPalette& theme = UITheme::Get();
    previewGizmoState.hoverAxis = -1;

    if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size()) {
        Rectangle bar = {area.x, area.y, area.width, 22};
        DrawRectangleRec(bar, theme.tabBar);
        DrawLine((int)bar.x, (int)(bar.y + bar.height), (int)(bar.x + bar.width), (int)(bar.y + bar.height), theme.panelBorder);

        std::string functionName = graph.GetFunctions()[openFunctionIndex].name;

        const int crumbFontSize = 12;
        float cursorX = bar.x + 6;

        {
            int labelW = MeasureText(graph.assetName.c_str(), crumbFontSize);
            Rectangle crumbRect = {cursorX, bar.y + 2, (float)labelW, 18};
            Color crumbColor = theme.textPrimary;
            bool crumbHovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), crumbRect);
            if (crumbHovered) crumbColor = theme.accent;
            if (crumbHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) CloseFunctionGraph();
            DrawText(graph.assetName.c_str(), (int)cursorX, (int)bar.y + 4, crumbFontSize, crumbColor);
            cursorX += (float)labelW;
        }

        {
            const char* separator = " / ";
            int sepW = MeasureText(separator, crumbFontSize);
            DrawText(separator, (int)cursorX, (int)bar.y + 4, crumbFontSize, theme.textSecondary);
            cursorX += (float)sepW;
        }

        DrawText(functionName.c_str(), (int)cursorX, (int)bar.y + 4, crumbFontSize, theme.textPrimary);

        area.y += bar.height;
        area.height -= bar.height;
    }

    graphArea = area;
    Vector2 mousePos = GetMousePosition();
    const bool mouseInGraph = CheckCollisionPointRec(mousePos, area) && !WidgetInput::IsMouseBlocked();
    auto pressedInGraph = [&](int button) { return mouseInGraph && IsMouseButtonPressed(button); };

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
                    for (Node& n : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
                        for (Pin& p : n.inputs) {
                            if (p.id == listeningForKeyPinId) { p.defaultValue = keyName; isDirty = true; }
                        }
                    }
                }
                listeningForKeyPinId = 0;
            } else if (pressedInGraph(MOUSE_BUTTON_LEFT)) {
                listeningForKeyPinId = 0;
            }
        }
    }

    GraphCanvas::UpdatePanZoom(canvasState, graphArea, mousePos, !nodeMenuOpen && !keyPickerOpen);

    GraphCanvas::DrawGrid(graphArea, canvasState.scroll, canvasState.zoom);

    BeginScissorMode((int)graphArea.x, (int)graphArea.y, (int)graphArea.width, (int)graphArea.height);

    for (const Link& link : ActiveGraph().GetLinks()) {
        const Pin* outPin = nullptr; const Node* outNode = nullptr;
        const Pin* inPin  = nullptr; const Node* inNode  = nullptr;
        for (const Node& node : ActiveGraph().GetNodes()) {
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
            if (pressedInGraph(MOUSE_BUTTON_LEFT)) {
                RemoveSpecificLink(link.outputPinId, link.inputPinId);
                break;
            }
        } else {
            GraphCanvas::DrawWire(from, to, 2.0f, wireColor);
        }
    }

    RenderWireInProgress();

    for (Node& node : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
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
                for (const Link& lk : ActiveGraph().GetLinks()) if (lk.inputPinId == targetPin->id) { targetLinked = true; break; }
            }
            bool showTargetPin = targetPin && (targetLinked || linkFromPin != -1 || CheckCollisionPointRec(mousePos, nodeBounds));

            if (showTargetPin && CheckCollisionPointCircle(mousePos, {nx + scaledRadius + 2, ny + scaledHeight * 0.5f}, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV({nx + scaledRadius + 2, ny + scaledHeight * 0.5f}, scaledRadius + 2, {255, 255, 255, 60});
                if (pressedInGraph(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                for (const Link& lk : ActiveGraph().GetLinks()) if (lk.outputPinId == outputPin->id) { linked = true; break; }
                DrawCircleV({nx + scaledNodeWidth - scaledRadius - 2, ny + scaledHeight * 0.5f}, scaledRadius + 2, {255, 255, 255, 60});
                if (pressedInGraph(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                for (const Link& lk : ActiveGraph().GetLinks()) if (lk.outputPinId == outputPin->id) { linked = true; break; }
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
                if (pressedInGraph(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                    lastValidVariableIndex = -1;
                    selectedFunctionIndex = -1;
                    lastValidFunctionIndex = -1;
                    selectedComponentIndex = -1;
                    lastValidComponentIndex = -1;
                    selectedLocalVariableIndex = -1;
                    lastValidLocalVariableIndex = -1;
                    draggingNode = true;
                    nodeDragOffset = {mousePos.x - nx, mousePos.y - ny};
                    break;
                }
            }
            
            if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !pressedInGraph(MOUSE_BUTTON_LEFT)) {
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
                    for (Node& other : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
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
            for (const Link& lk : ActiveGraph().GetLinks()) if (lk.inputPinId == pin.id) { linked = true; break; }

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
                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                    Rectangle boolRect = {fieldRect.x, fieldRect.y + 1, fieldRect.height - 2, fieldRect.height - 2};
                    bool boolHovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mousePos, boolRect);
                    DrawRectangleRec(boolRect, boolHovered ? Color{55,55,60,255} : Color{40,40,45,255});
                    DrawRectangleLinesEx(boolRect, 1, {90,90,95,255});
                    if (bval) {
                        float pad = boolRect.width * 0.22f;
                        DrawRectangleRec({boolRect.x+pad, boolRect.y+pad, boolRect.width-pad*2, boolRect.height-pad*2}, {90,140,220,255});
                    }
                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && boolHovered) {
                        pin.defaultValue = !bval ? "true" : "false";
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

                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && !nodeMenuOpen) {
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

                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::FunctionName;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
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

                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::ComponentRef;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
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

                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        actorClassPickerPinId = pin.id;
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

                    if (pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                        keyPickerKind = PinPickerKind::MappingName;
                        keyPickerPinId = pin.id;
                        keyPickerPos = {fieldRect.x, fieldRect.y + fieldRect.height};
                        keyPickerWidth = fieldRect.width;
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

                    if (!pinEditChrome) {
                        pinEditChrome = new Widget();
                        auto* root = new WidgetElement();
                        pinEditChrome->SetRoot(root);
                        pinEditField = new TextElement();
                        pinEditField->editable = true;
                        pinEditField->fontSize = 11.0f;
                        pinEditField->backgroundColor = {35, 35, 40, 255};
                        pinEditField->borderColor = {60, 60, 70, 255};
                        pinEditField->editingBorderColor = UITheme::Get().accent;
                        pinEditField->onTextCommitted = [this](const std::string& text) {
                            Pin* p = ActiveGraph().FindPin(editingPinId);
                            if (p) { p->defaultValue = text; isDirty = true; }
                            editingPinId = 0;
                        };
                        root->AddChild(pinEditField);
                    }

                    if (allowActivate && editingPinId == pin.id) {
                        pinEditField->size = { fieldRect.width, fieldRect.height };
                        pinEditField->offset = { fieldRect.x, fieldRect.y };
                        pinEditField->placeholder = targetPlaceholder ? targetPlaceholder : "";
                        pinEditActiveThisFrame = true;
                        if (!pinEditField->isEditing) {
                            pinEditField->text = pin.defaultValue;
                            pinEditField->StartEditing();
                        }
                    } else {
                        std::string display = (pin.defaultValue.empty() && targetPlaceholder) ? targetPlaceholder : pin.defaultValue;
                        DrawRectangleRec(fieldRect, {35, 35, 40, 255});
                        DrawRectangleLinesEx(fieldRect, 1, {60, 60, 70, 255});
                        DrawText(display.c_str(), (int)fieldRect.x + 6, (int)fieldRect.y + 5, 11,
                                 pin.defaultValue.empty() ? Color{140,140,150,255} : WHITE);
                        if (allowActivate && pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, fieldRect) && !nodeMenuOpen) {
                            editingPinId = pin.id;
                            keyPickerOpen = false;
                            keyPickerPinId = 0;
                            listeningForKeyPinId = 0;
                        }
                    }
                }
                inputY += scaledPinHeight;
            }

            if (CheckCollisionPointCircle(mousePos, pinPos, scaledRadius + 4) && CheckCollisionPointRec(mousePos, graphArea)) {
                DrawCircleV(pinPos, scaledRadius + 2, {255, 255, 255, 60});
                if (pressedInGraph(MOUSE_BUTTON_LEFT)) {
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
            for (const Link& lk : ActiveGraph().GetLinks()) if (lk.outputPinId == pin.id) { linked = true; break; }

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
                if (pressedInGraph(MOUSE_BUTTON_LEFT) && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
            if (pressedInGraph(MOUSE_BUTTON_LEFT) && linkFromPin == -1 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                lastValidVariableIndex = -1;
                selectedFunctionIndex = -1;
                lastValidFunctionIndex = -1;
                selectedComponentIndex = -1;
                lastValidComponentIndex = -1;
                selectedLocalVariableIndex = -1;
                lastValidLocalVariableIndex = -1;
                draggingNode = true;
                nodeDragOffset = {mousePos.x - nx, mousePos.y - ny};
                break;
            }
        }

        if (draggingNode && selectedNodeId == nodeIdInt && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !pressedInGraph(MOUSE_BUTTON_LEFT)) {
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
                for (Node& other : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
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
            for (const Node& node : ActiveGraph().GetNodes()) {
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
                nodeMenuSearch.clear();
                nodeMenuHighlighted = 0;
            }
            CancelLink();
        }
    }

    if (keyPickerOpen) {
        RenderKeyPickerPopup();
    }

    EndScissorMode();

    bool varDragIsLocal = DragDrop::Matches("StargraphLocalVariable");
    if (DragDrop::Matches("StargraphVariable") || varDragIsLocal) {
        Vector2 mp = GetMousePosition();
        Stargraph& sourceGraph = varDragIsLocal ? ActiveGraph() : graph;
        int varIndex = atoi(DragDrop::Payload().c_str());
        if (varIndex >= 0 && varIndex < (int)sourceGraph.GetVariables().size()) {
            const Variable& var = sourceGraph.GetVariables()[varIndex];
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            bool alt  = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            std::string hint = "  (Alt=Set, Ctrl=Save, Ctrl+Alt=Load Saved, Shift=Get Saved)";
            DrawText(hint.c_str(), (int)(mp.x + 18), (int)(mp.y + 18), 12, {130, 130, 140, 255});

            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mp, graphArea)) {
                    Node* node = nullptr;
                    if (ctrl && alt) {
                        node = ActiveGraph().CreateNode("Load_Saved_Variable");
                        node->title = "Load Saved: " + var.name;
                        for (Pin& p : node->inputs) {
                            if (p.name == "Variable_Name") p.defaultValue = var.name;
                        }
                        for (Pin& p : node->outputs) {
                            if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; }
                        }
                    } else if (ctrl) {
                        node = ActiveGraph().CreateNode("Save_Variable");
                        node->title = "Save: " + var.name;
                        for (Pin& p : node->inputs) {
                            if (p.name == "Variable_Name") p.defaultValue = var.name;
                        }
                    } else if (shift) {
                        node = ActiveGraph().CreateNode("Get_Saved_Variable");
                        node->title = "Get Saved: " + var.name;
                        for (Pin& p : node->inputs) {
                            if (p.name == "Variable_Name") p.defaultValue = var.name;
                        }
                        for (Pin& p : node->outputs) {
                            if (p.name == "Value") { p.type = var.type; p.objectKind = var.objectKind; }
                        }
                    } else if (alt) {
                        node = ActiveGraph().CreateNode("Set_Variable");
                        node->title = "Set: " + var.name;
                        for (Pin& p : node->inputs) {
                            if (p.name == "Variable_Name") p.defaultValue = var.name;
                            if (p.name == "Value")         { p.type = var.type; p.objectKind = var.objectKind; }
                        }
                    } else {
                        node = ActiveGraph().CreateNode("Get_Variable");
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
                DragDrop::Consume();
            }
        }
    }

    if (DragDrop::Matches("StargraphFunction")) {
        Vector2 mp = GetMousePosition();
        int funcIndex = atoi(DragDrop::Payload().c_str());
        if (funcIndex >= 0 && funcIndex < (int)graph.GetFunctions().size()) {
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mp, graphArea)) {
                    const FunctionDef& func = graph.GetFunctions()[funcIndex];
                    Node* node = ActiveGraph().CreateNode("Call_Function");
                    node->title = "Call: " + func.name;
                    for (Pin& p : node->inputs) {
                        if (p.name == "Function Name") p.defaultValue = func.name;
                    }
                    Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, mp));
                    node->posX = snapped.x;
                    node->posY = snapped.y;
                    isDirty = true;
                }
                DragDrop::Consume();
            }
        }
    }

    if (DragDrop::Matches("StargraphComponent")) {
        Vector2 mp = GetMousePosition();
        const std::string& compName = DragDrop::Payload();
        const ComponentDecl* comp = nullptr;
        for (const ComponentDecl& c : graph.GetComponents()) {
            if (c.name == compName) { comp = &c; break; }
        }
        if (comp && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = ActiveGraph().CreateNode("Get_Component");
                node->title = "Get: " + comp->name;
                node->componentClass = comp->componentType;
                for (Pin& p : node->inputs) {
                    if (p.name == "Component_Name") p.defaultValue = comp->name;
                }
                Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, mp));
                node->posX = snapped.x;
                node->posY = snapped.y;
                isDirty = true;
            }
            DragDrop::Consume();
        }
    }

    if (elementDragActive) {
        Vector2 mp = GetMousePosition();
        std::string label = "Get: " + elementDragName;
        int w = (int)MeasureText(label.c_str(), 12) + 12;
        DrawRectangleRec({mp.x + 12, mp.y - 4, (float)w, 20}, {60, 60, 65, 255});
        DrawRectangleLinesEx({mp.x + 12, mp.y - 4, (float)w, 20}, 1, {100, 100, 110, 255});
        DrawText(label.c_str(), (int)(mp.x + 18), (int)(mp.y), 12, WHITE);

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mp, graphArea)) {
                Node* node = ActiveGraph().CreateNode("Get_Widget_Element");
                node->title = "Get: " + elementDragName;
                node->componentClass = elementDragClass;
                for (Pin& p : node->inputs) {
                    if (p.name == "Element_Name") p.defaultValue = elementDragName;
                }
                for (Pin& p : node->outputs) {
                    if (p.name == "Element") p.objectKind = elementDragClass;
                }
                Vector2 snapped = GraphCanvas::SnapToGrid(GraphCanvas::ScreenToWorld(canvasState, graphArea, mp));
                node->posX = snapped.x;
                node->posY = snapped.y;
                isDirty = true;
            }
            elementDragActive = false;
        }
    }

    if (pressedInGraph(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, graphArea) && linkFromPin == -1 && !draggingNode && editingPinId == 0 && !keyPickerOpen && !nodeMenuOpen && !actorClassPicker.IsOpen()) {
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
                for (const Node& node : ActiveGraph().GetNodes()) {
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
        nodeMenuSearch.clear();
        nodeMenuHighlighted = 0;
        }
    }

    if (pinEditChrome) {
        if (!pinEditActiveThisFrame && pinEditField->isEditing) {
            pinEditField->OnDeselected();
        }
        Rectangle pinEditBounds = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        float pinEditDt = GetFrameTime();
        pinEditChrome->Tick(pinEditDt, pinEditBounds, 1.0f);
        if (pinEditActiveThisFrame) {
            WidgetInput::ProcessInput(pinEditChrome, WidgetInput::kEditorDevice, pinEditDt, GetMousePosition(), 1.0f);
            pinEditChrome->Draw(pinEditBounds, 1.0f);
        }
    }
    pinEditActiveThisFrame = false;

    const bool graphKeys = WidgetInput::ScopeHasKeyboard();

    if (graphKeys && IsKeyPressed(KEY_DELETE) && selectedNodeId != -1) DeleteSelected();

    if (graphKeys && IsKeyDown(KEY_LEFT_CONTROL) && editingPinId == 0) {
        if (IsKeyPressed(KEY_C)) CopySelectedNodes();
        else if (IsKeyPressed(KEY_V)) PasteNodes();
        else if (IsKeyPressed(KEY_D)) DuplicateSelected();
    }

    if (graphKeys && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S) && isDirty) {
        SaveGraph();
    }
}

void StargraphEditorPanel::SpawnNodeFromMenu(const std::string& nodeTypeIdIn, const std::string& sourceKind) {
    std::string nodeTypeId = nodeTypeIdIn;
    std::string nodeType = nodeTypeId;
    std::string eventName;
    std::string functionName;
    std::string mappingName;
    std::string externalVarName;
    std::string externalCompName;
    std::string boundElementId;
    bool isExternalSet = false;
    if (nodeType.rfind(kBoundElementEventPrefix, 0) == 0) {
        std::string rest = nodeType.substr(kBoundElementEventPrefix.size());
        size_t separator = rest.find(':');
        if (separator != std::string::npos) {
            boundElementId = rest.substr(separator + 1);
            nodeType = rest.substr(0, separator);
        }
    } else if (nodeType.rfind("Call_Custom_Event:", 0) == 0) {
        eventName = nodeType.substr(18);
        nodeType = "Call_Custom_Event";
    } else if (nodeType.rfind("Call_Function:", 0) == 0) {
        functionName = nodeType.substr(14);
        nodeType = "Call_Function";
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
    Node* node = ActiveGraph().CreateNode(nodeType);
    if (!boundElementId.empty()) {
        for (Pin& p : node->inputs) {
            if (p.name == "Element") { p.defaultValue = boundElementId; break; }
        }
        node->title = NodeMenuDisplayLabel(nodeTypeId);
    } else if (const NodeTemplate* spawnedTemplate = NodeRegistry::Get().GetTemplate(nodeType)) {
        if (!spawnedTemplate->menuLabel.empty()) node->title = spawnedTemplate->menuLabel;
    }
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
                ActiveGraph().AddDynamicPin(*node, false, srcPin.name, srcPin.type);
                keep.push_back(srcPin.name);
            }
            std::vector<std::string> toRemove;
            for (Pin& p : node->inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keep) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemove.push_back(p.name);
            }
            for (const std::string& n : toRemove) ActiveGraph().RemoveDynamicPin(*node, false, n);
        }
    }
    if (!functionName.empty()) {
        for (Pin& p : node->inputs) {
            if (p.name == "Function Name") {
                p.defaultValue = functionName;
                break;
            }
        }
        if (FunctionDef* targetFn = graph.FindFunction(functionName)) {
            Node* entryNode = targetFn->graph.FindNodeByType("Function_Entry");
            Node* returnNode = targetFn->graph.FindNodeByType("Return");

            std::vector<std::string> keepInputs = {"Function Name"};
            if (entryNode) {
                for (const Pin& srcPin : entryNode->outputs) {
                    if (srcPin.type == PinType::Exec) continue;
                    ActiveGraph().AddDynamicPin(*node, false, srcPin.name, srcPin.type);
                    keepInputs.push_back(srcPin.name);
                }
            }
            std::vector<std::string> toRemoveIn;
            for (Pin& p : node->inputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keepInputs) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemoveIn.push_back(p.name);
            }
            for (const std::string& n : toRemoveIn) ActiveGraph().RemoveDynamicPin(*node, false, n);

            std::vector<std::string> keepOutputs;
            if (returnNode) {
                for (const Pin& srcPin : returnNode->inputs) {
                    if (srcPin.type == PinType::Exec) continue;
                    ActiveGraph().AddDynamicPin(*node, true, srcPin.name, srcPin.type);
                    keepOutputs.push_back(srcPin.name);
                }
            }
            std::vector<std::string> toRemoveOut;
            for (Pin& p : node->outputs) {
                if (p.type == PinType::Exec) continue;
                bool keepIt = false;
                for (const std::string& k : keepOutputs) if (k == p.name) { keepIt = true; break; }
                if (!keepIt) toRemoveOut.push_back(p.name);
            }
            for (const std::string& n : toRemoveOut) ActiveGraph().RemoveDynamicPin(*node, true, n);
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
        for (const Node& n : ActiveGraph().GetNodes()) {
            for (const Pin& pin : n.outputs) if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = true; fromNode = &n; }
            for (const Pin& pin : n.inputs)  if (pin.id == filterCompatibleWithPin) { fromPin = &pin; fromIsOutput = false; fromNode = &n; }
        }
        if (fromPin) {
            std::string sourceKind = EffectivePinObjectKind(fromNode, *fromPin);
            if (fromIsOutput) {
                for (const Pin& p : node->inputs) {
                    if (p.type == PinType::Wildcard) { ActiveGraph().AddLink({filterCompatibleWithPin, p.id}); break; }
                    if (p.type != fromPin->type) continue;
                    if (p.type == PinType::Object) {
                        if (!ObjectKindRegistry::Get().IsCompatible(sourceKind, p.objectKind)) continue;
                    }
                    ActiveGraph().AddLink({filterCompatibleWithPin, p.id});
                    break;
                }
            } else {
                for (const Pin& p : node->outputs) {
                    if (p.type == PinType::Wildcard) { ActiveGraph().AddLink({p.id, filterCompatibleWithPin}); break; }
                    if (p.type != fromPin->type) continue;
                    if (p.type == PinType::Object) {
                        if (!ObjectKindRegistry::Get().IsCompatible(p.objectKind, sourceKind)) continue;
                    }
                    ActiveGraph().AddLink({p.id, filterCompatibleWithPin});
                    break;
                }
            }
            RefreshWildcardPins(node);
            RefreshWildcardPins(ActiveGraph().FindNodeByPinId(filterCompatibleWithPin));
        }
    }

    isDirty = true;
    CloseNodeMenu();
}

void StargraphEditorPanel::RenderNodeContextMenu() {
    if (!nodeMenuOpen) return;
    bool nodeMenuWasJustOpened = nodeMenuJustOpened;

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
        for (Node& node : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
            for (Pin& pin : node.outputs) if (pin.id == filterCompatibleWithPin) { dragFromPin = &pin; dragFromIsOutput = true; dragSourceNode = &node; break; }
            for (Pin& pin : node.inputs)  if (pin.id == filterCompatibleWithPin) { dragFromPin = &pin; dragFromIsOutput = false; dragSourceNode = &node; break; }
        }
        if (dragFromPin) sourceKind = EffectivePinObjectKind(dragSourceNode, *dragFromPin);
    }

    for (const auto& t : templates) {
        if (t.type == "Get_Variable" || t.type == "Set_Variable") continue;
        if (t.type == "Call_Custom_Event") continue;
        if (t.type == "Call_Function") continue;
        if (t.type == "Input_Mapping") continue;
        if (t.type == "Function_Entry") continue;
        if (t.type == "Return") continue;
        if (!t.boundElementClass.empty()) continue;
        if (t.type == "Get_Parent_Actor_From_Widget" && !listWidgetElements) continue;
        if (!t.boundComponentType.empty()) {
            bool graphHasComponent = false;
            for (const ComponentDecl& comp : graph.GetComponents()) {
                if (comp.componentType == t.boundComponentType) { graphHasComponent = true; break; }
            }
            if (!graphHasComponent) continue;
        }

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

    for (const FunctionDef& fn : graph.GetFunctions()) {
        if (fn.name.empty()) continue;

        std::string typeLower = NodeMenuDisplayLabel("Call_Function:" + fn.name);
        for (char& c : typeLower) c = (char)tolower(c);
        if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

        entries.push_back({"Functions", "Call_Function:" + fn.name});
    }

    if (openFunctionIndex >= 0) {
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate("Return");
        if (tmpl) {
            std::string typeLower = NodeMenuDisplayLabel(tmpl->type);
            for (char& c : typeLower) c = (char)tolower(c);
            if (searchLower.empty() || typeLower.find(searchLower) != std::string::npos) {
                entries.push_back({"Functions", tmpl->type});
            }
        }
    }

    for (const ComponentDecl& comp : graph.GetComponents()) {
        if (comp.componentType != "InputComponent") continue;
        auto mappingIt = comp.arrayFields.find("InputMapping");
        if (mappingIt == comp.arrayFields.end()) continue;
        for (const std::string& mappingName : mappingIt->second) {
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

    if (listWidgetElements && filterCompatibleWithPin == -1) {
        for (const auto& element : listWidgetElements()) {
            if (!ObjectEvents::GetWidgetElementEvents(element.second)) continue;

            std::string nodeId = kBoundElementEventPrefix + ObjectEvents::WidgetElementEventNodeType(element.second) + ":" + element.first;
            std::string typeLower = NodeMenuDisplayLabel(nodeId);
            for (char& c : typeLower) c = (char)tolower(c);
            if (!searchLower.empty() && typeLower.find(searchLower) == std::string::npos) continue;

            bool alreadyListed = false;
            for (const auto& e : entries) if (e.second == nodeId) { alreadyListed = true; break; }
            if (!alreadyListed) entries.push_back({"Widget Events", nodeId});
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

    if (entries.empty() && searchLower.empty()) { CloseNodeMenu(); return; }

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
            CloseNodeMenu(); return;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            CloseNodeMenu(); return;
        }
    }

    DrawRectangleRec(popup, UITheme::Get().panelBg);
    DrawRectangleLinesEx(popup, 1, UITheme::Get().panelBorder);

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    if (!nodeMenuChrome) {
        nodeMenuChrome = new Widget();
        auto* root = new WidgetElement();
        nodeMenuChrome->SetRoot(root);
        nodeMenuSearchField = new TextElement();
        nodeMenuSearchField->editable = true;
        nodeMenuSearchField->fontSize = 12.0f;
        nodeMenuSearchField->backgroundColor = UITheme::Get().fieldBg;
        nodeMenuSearchField->borderColor = UITheme::Get().fieldBorder;
        nodeMenuSearchField->editingBorderColor = UITheme::Get().accent;
        nodeMenuSearchField->onTextChanged = [this](const std::string& text) {
            nodeMenuSearch = text;
            nodeMenuHighlighted = 0;
        };
        root->AddChild(nodeMenuSearchField);
    }
    if (nodeMenuWasJustOpened) {
        nodeMenuSearchField->text.clear();
        nodeMenuHighlighted = 0;
    }
    nodeMenuSearchField->size = { searchRect.width, searchRect.height };
    nodeMenuSearchField->offset = { searchRect.x, searchRect.y };
    float nodeMenuDt = GetFrameTime();
    nodeMenuChrome->Tick(nodeMenuDt, { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, 1.0f);
    if (!nodeMenuSearchField->isEditing) nodeMenuSearchField->StartEditing();
    WidgetInput::ProcessInput(nodeMenuChrome, WidgetInput::kEditorDevice, nodeMenuDt, GetMousePosition(), 1.0f);
    nodeMenuChrome->Draw({ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, 1.0f);

    if (IsKeyPressed(KEY_DOWN) && nodeMenuHighlighted + 1 < (int)entries.size()) nodeMenuHighlighted++;
    if (IsKeyPressed(KEY_UP) && nodeMenuHighlighted > 0) nodeMenuHighlighted--;
    if (nodeMenuHighlighted >= (int)entries.size()) nodeMenuHighlighted = (int)entries.size() - 1;
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && nodeMenuHighlighted >= 0 && nodeMenuHighlighted < (int)entries.size()) {
        SpawnNodeFromMenu(entries[nodeMenuHighlighted].second, sourceKind);
    }

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
        int entryIndex = (int)(&entry - &entries[0]);
        bool isHighlighted = (entryIndex == nodeMenuHighlighted);
        bool isHovered = CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea);
        if (isHovered || isHighlighted) {
            DrawRectangleRec(btn, theme.surfaceHover);
            if (isHovered) nodeMenuHighlighted = entryIndex;
            if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                SpawnNodeFromMenu(entry.second, sourceKind);
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
