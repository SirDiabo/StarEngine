#include "Stargraph.h"
#include "StargraphArray.h"
#include "NodeRegistry.h"
#include "Runtime/ComponentRegistry.h"
#include "Runtime/ActorAsset.h"
#include "Core/CookedFile.h"
#include "Core/SafeParse.h"
#include "Core/ObjectKindRegistry.h"
#include "raylib.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

static constexpr int kMaxDynamicWildcardInputs = 256;

static Pin MakeExecOutputPin(const std::string& name, int id) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Exec;
    pin.isInput = false;
    pin.isOutput = true;
    pin.id = id;
    return pin;
}

static std::string ExecCasePinName(const NodeTemplate& tmpl, const std::string& spec, int index) {
    return tmpl.execCasePinName ? tmpl.execCasePinName(spec, index) : std::to_string(index);
}

static std::string EscapeCaseText(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

static std::string UnescapeCaseText(const std::string& text) {
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

static std::string ResolveInputPinAlias(const std::string& nodeType, const std::string& pinName) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(nodeType);
    if (!tmpl) return pinName;
    auto it = tmpl->legacyInputPinAliases.find(pinName);
    return it != tmpl->legacyInputPinAliases.end() ? it->second : pinName;
}

static std::string ResolveOutputPinAlias(const std::string& nodeType, const std::string& pinName) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(nodeType);
    if (!tmpl) return pinName;
    auto it = tmpl->legacyOutputPinAliases.find(pinName);
    return it != tmpl->legacyOutputPinAliases.end() ? it->second : pinName;
}

static Pin* FindExecCasePin(Node& node, const NodeTemplate& tmpl, int caseIndex) {
    int execIndex = (tmpl.dynamicExecFixedPin.empty() ? 0 : 1) + caseIndex;
    int seen = 0;
    for (Pin& pin : node.outputs) {
        if (pin.type != PinType::Exec) continue;
        if (seen == execIndex) return &pin;
        seen++;
    }
    return nullptr;
}

static bool ExecOutputNameTaken(const Node& node, const std::string& name, const Pin* ignore) {
    for (const Pin& pin : node.outputs) {
        if (&pin == ignore || pin.type != PinType::Exec) continue;
        if (pin.name == name) return true;
    }
    return false;
}

void Stargraph::RebuildDynamicExecOutputs(Node& node) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->dynamicExecOutputs) return;

    std::vector<int> staleIds;
    for (const Pin& pin : node.outputs) if (pin.type == PinType::Exec) staleIds.push_back(pin.id);
    for (int id : staleIds) RemoveLink(id);
    node.outputs.erase(std::remove_if(node.outputs.begin(), node.outputs.end(),
        [](const Pin& pin) { return pin.type == PinType::Exec; }), node.outputs.end());

    if (!tmpl->dynamicExecFixedPin.empty()) node.outputs.push_back(MakeExecOutputPin(tmpl->dynamicExecFixedPin, nextPinId++));
    for (size_t i = 0; i < node.execCases.size(); i++) {
        node.outputs.push_back(MakeExecOutputPin(ExecCasePinName(*tmpl, node.execCases[i], (int)i), nextPinId++));
    }
    InvalidateCaches();
}

bool Stargraph::AddExecCase(Node& node, const std::string& input) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->dynamicExecOutputs) return false;

    std::string spec;
    if (tmpl->normalizeExecCase && !tmpl->normalizeExecCase(input, spec)) return false;

    int index = (int)node.execCases.size();
    std::string name = ExecCasePinName(*tmpl, spec, index);
    if (ExecOutputNameTaken(node, name, nullptr)) return false;

    node.execCases.push_back(spec);
    node.outputs.push_back(MakeExecOutputPin(name, nextPinId++));
    InvalidateCaches();
    return true;
}

bool Stargraph::RemoveExecCase(Node& node, int caseIndex) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->dynamicExecOutputs) return false;
    if (caseIndex < 0 || caseIndex >= (int)node.execCases.size()) return false;
    if ((int)node.execCases.size() <= tmpl->dynamicExecMinCases) return false;

    Pin* pin = FindExecCasePin(node, *tmpl, caseIndex);
    if (!pin) return false;
    int pinId = pin->id;
    RemoveLink(pinId);
    node.outputs.erase(std::remove_if(node.outputs.begin(), node.outputs.end(),
        [pinId](const Pin& p) { return p.id == pinId; }), node.outputs.end());
    node.execCases.erase(node.execCases.begin() + caseIndex);

    for (size_t i = 0; i < node.execCases.size(); i++) {
        if (Pin* p = FindExecCasePin(node, *tmpl, (int)i)) p->name = ExecCasePinName(*tmpl, node.execCases[i], (int)i);
    }
    InvalidateCaches();
    return true;
}

bool Stargraph::SetExecCase(Node& node, int caseIndex, const std::string& input) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !tmpl->dynamicExecOutputs || !tmpl->normalizeExecCase) return false;
    if (caseIndex < 0 || caseIndex >= (int)node.execCases.size()) return false;

    std::string spec;
    if (!tmpl->normalizeExecCase(input, spec)) return false;

    Pin* pin = FindExecCasePin(node, *tmpl, caseIndex);
    if (!pin) return false;
    std::string name = ExecCasePinName(*tmpl, spec, caseIndex);
    if (ExecOutputNameTaken(node, name, pin)) return false;

    node.execCases[caseIndex] = spec;
    pin->name = name;
    InvalidateCaches();
    return true;
}

Node* Stargraph::CreateNode(const std::string& type) {
    InvalidateCaches();

    nodes.emplace_back();
    Node* node = &nodes.back();
    node->id = std::to_string(nextNodeId++);
    node->type = type;
    node->title = type;

    const NodeTemplate* nodeTemplate = NodeRegistry::Get().GetTemplate(type);
    if (nodeTemplate && nodeTemplate->createPins) {
        nodeTemplate->createPins(node);
    }

    if (nodeTemplate && nodeTemplate->dynamicExecOutputs) {
        node->execCases = nodeTemplate->dynamicExecInitialCases;
        if (!nodeTemplate->dynamicExecFixedPin.empty()) node->outputs.push_back(MakeExecOutputPin(nodeTemplate->dynamicExecFixedPin, 0));
        for (size_t i = 0; i < node->execCases.size(); i++) {
            node->outputs.push_back(MakeExecOutputPin(ExecCasePinName(*nodeTemplate, node->execCases[i], (int)i), 0));
        }
    }

    if (nodeTemplate && HasDynamicWildcardInputs(*nodeTemplate)) {
        for (int i = 0; i < nodeTemplate->dynamicWildcardInputInitial; i++) {
            Pin pin;
            pin.name = DynamicWildcardInputName(*nodeTemplate, i);
            pin.type = PinType::Wildcard;
            pin.isInput = true;
            pin.isOutput = false;
            node->inputs.push_back(pin);
        }
    }

    for (Pin& pin : node->inputs)  pin.isWildcard = (pin.type == PinType::Wildcard);
    for (Pin& pin : node->outputs) pin.isWildcard = (pin.type == PinType::Wildcard);

    for (Pin& pin : node->inputs) pin.id = nextPinId++;
    for (Pin& pin : node->outputs) pin.id = nextPinId++;

    return node;
}

void Stargraph::AddLink(const Link& link) {
    Link newLink = link;
    if (newLink.id.empty()) newLink.id = std::to_string(nextLinkId++);
    links.push_back(newLink);
    InvalidateCaches();
}

void Stargraph::RemoveLink(int pinId) {
    for (auto it = links.begin(); it != links.end();) {
        if (it->inputPinId == pinId || it->outputPinId == pinId) {
            it = links.erase(it);
        } else {
            ++it;
        }
    }
    InvalidateCaches();
}

void Stargraph::RemoveSpecificLink(int outputPinId, int inputPinId) {
    for (auto it = links.begin(); it != links.end(); ++it) {
        if (it->outputPinId == outputPinId && it->inputPinId == inputPinId) {
            links.erase(it);
            InvalidateCaches();
            return;
        }
    }
}

bool Stargraph::HasLink(int outputPinId, int inputPinId) const {
    for (const Link& link : links) {
        if (link.outputPinId == outputPinId && link.inputPinId == inputPinId) return true;
    }
    return false;
}

Node* Stargraph::InsertRerouteOnLink(int outputPinId, int inputPinId, float posX, float posY) {
    if (!HasLink(outputPinId, inputPinId)) return nullptr;
    Node* reroute = CreateNode("Reroute");
    if (!reroute) return nullptr;
    if (reroute->inputs.empty() || reroute->outputs.empty()) {
        RemoveNode(std::stoi(reroute->id));
        return nullptr;
    }
    reroute->posX = posX;
    reroute->posY = posY;
    const int rerouteIn = reroute->inputs[0].id;
    const int rerouteOut = reroute->outputs[0].id;
    RemoveSpecificLink(outputPinId, inputPinId);
    AddLink({outputPinId, rerouteIn});
    AddLink({rerouteOut, inputPinId});
    RefreshWildcardPins(reroute);
    return reroute;
}

std::vector<int> Stargraph::ReplaceLinksForConnection(int outputPinId, int inputPinId) {
    std::vector<int> affected;
    const Pin* out = FindPin(outputPinId);
    const Pin* in = FindPin(inputPinId);
    if (!out || !in) return affected;

    const bool replaceInputLinks = in->type != PinType::Exec;
    const bool replaceOutputLinks = out->type == PinType::Exec;
    for (auto it = links.begin(); it != links.end();) {
        const bool dropForInput = replaceInputLinks && it->inputPinId == inputPinId;
        const bool dropForOutput = replaceOutputLinks && it->outputPinId == outputPinId;
        if (!dropForInput && !dropForOutput) {
            ++it;
            continue;
        }
        if (dropForInput) affected.push_back(it->outputPinId);
        if (dropForOutput) affected.push_back(it->inputPinId);
        it = links.erase(it);
    }
    if (!affected.empty()) InvalidateCaches();
    return affected;
}

bool Stargraph::IsPinHidden(const Node& node, const Pin& pin) const {
    if (!pin.collapsed || node.expanded) return false;
    for (const Link& link : links) {
        if (link.inputPinId == pin.id || link.outputPinId == pin.id) return false;
    }
    return true;
}

bool Stargraph::HasCollapsiblePins(const Node& node) const {
    for (const Pin& pin : node.inputs) if (pin.collapsed) return true;
    for (const Pin& pin : node.outputs) if (pin.collapsed) return true;
    return false;
}

std::string Variable::DefaultElementText() const {
    return StarArray::DefaultElementText(type);
}

void Stargraph::NormalizeNodePins(Node& node) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (tmpl && tmpl->normalizePins) tmpl->normalizePins(*this, node);
}

bool Stargraph::ResolveNodeDetailTarget(const Node& node, Stargraph& host, int& kind, int& index) {
    if (node.type == "Call_Function") {
        std::string name;
        for (const Pin& p : node.inputs) if (p.name == "Function Name") name = p.defaultValue.c_str();
        for (size_t i = 0; i < functions.size(); i++) {
            if (functions[i].name == name) { kind = 0; index = (int)i; return true; }
        }
        return false;
    }
    if (node.type == "Get_Variable" || node.type == "Set_Variable") {
        std::string name;
        bool external = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Variable_Name") name = p.defaultValue.c_str();
            if (p.name == "Target" && (!p.defaultValue.empty() || host.GetLinkToPin(p.id))) external = true;
        }
        if (external || name.empty()) return false;
        if (&host != this) {
            const std::vector<Variable>& locals = host.GetVariables();
            for (size_t i = 0; i < locals.size(); i++) {
                if (locals[i].name == name) { kind = 2; index = (int)i; return true; }
            }
        }
        for (size_t i = 0; i < variables.size(); i++) {
            if (variables[i].name == name) { kind = 1; index = (int)i; return true; }
        }
    }
    return false;
}

void Stargraph::RefreshCallEventPins(Node& callNode, Stargraph& host) {
    std::string targetName;
    for (const Pin& p : callNode.inputs) if (p.name == "Event Name") targetName = p.defaultValue.c_str();
    Node* targetEvent = FindCustomEventByName(targetName);
    if (!targetEvent) return;
    std::vector<std::string> keep = { "Target", "Event Name" };
    for (const Pin& srcPin : targetEvent->outputs) {
        if (srcPin.type == PinType::Exec) continue;
        host.AddDynamicPin(callNode, false, srcPin.name, srcPin.type);
        keep.push_back(srcPin.name);
    }
    std::vector<std::string> toRemove;
    for (const Pin& p : callNode.inputs) {
        if (p.type == PinType::Exec) continue;
        if (std::find(keep.begin(), keep.end(), p.name) == keep.end()) toRemove.push_back(p.name);
    }
    for (const std::string& name : toRemove) host.RemoveDynamicPin(callNode, false, name);
}

void Stargraph::ApplyCallFunctionPurity(Node& callNode, bool pure) {
    auto isExecNamed = [](const Pin& p, const char* name) { return p.type == PinType::Exec && p.name == name; };

    bool hasIn = false;
    bool hasOut = false;
    for (const Pin& p : callNode.inputs)  if (isExecNamed(p, "In"))  hasIn = true;
    for (const Pin& p : callNode.outputs) if (isExecNamed(p, "Out")) hasOut = true;
    if (pure ? (!hasIn && !hasOut) : (hasIn && hasOut)) return;

    if (pure) {
        for (auto it = callNode.inputs.begin(); it != callNode.inputs.end();) {
            if (isExecNamed(*it, "In")) { RemoveLink(it->id); it = callNode.inputs.erase(it); }
            else ++it;
        }
        for (auto it = callNode.outputs.begin(); it != callNode.outputs.end();) {
            if (isExecNamed(*it, "Out")) { RemoveLink(it->id); it = callNode.outputs.erase(it); }
            else ++it;
        }
    } else {
        if (!hasIn) {
            Pin pin;
            pin.name = "In";
            pin.type = PinType::Exec;
            pin.isInput = true;
            pin.id = nextPinId++;
            callNode.inputs.insert(callNode.inputs.begin(), pin);
        }
        if (!hasOut) {
            Pin pin;
            pin.name = "Out";
            pin.type = PinType::Exec;
            pin.isOutput = true;
            pin.id = nextPinId++;
            callNode.outputs.insert(callNode.outputs.begin(), pin);
        }
    }
    InvalidateCaches();
}

void Stargraph::SyncFunctionCallNodes(const std::string& functionName) {
    const FunctionDef* target = FindFunction(functionName);
    if (!target) return;
    auto syncGraph = [&](Stargraph& g) {
        for (Node& node : g.nodes) {
            if (node.type != "Call_Function") continue;
            for (const Pin& p : node.inputs) {
                if (p.name == "Function Name" && p.defaultValue == functionName) {
                    g.ApplyCallFunctionPurity(node, target->pure);
                    break;
                }
            }
        }
    };
    syncGraph(*this);
    for (FunctionDef& fn : functions) syncGraph(fn.graph);
}

bool Stargraph::HasLatentNodes() const {
    for (const Node& node : nodes) {
        const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
        if (tmpl && tmpl->latent) return true;
    }
    return false;
}

void Stargraph::RefreshCallFunctionPins(Node& callNode, Stargraph& host) {
    std::string targetName;
    for (const Pin& p : callNode.inputs) if (p.name == "Function Name") targetName = p.defaultValue.c_str();
    FunctionDef* targetFn = FindFunction(targetName);
    if (!targetFn) return;
    host.ApplyCallFunctionPurity(callNode, targetFn->pure);
    Node* entryNode = targetFn->graph.FindNodeByType("Function_Entry");
    Node* returnNode = targetFn->graph.FindNodeByType("Return");

    std::vector<std::string> keepInputs = { "Function Name" };
    if (entryNode) {
        for (const Pin& srcPin : entryNode->outputs) {
            if (srcPin.type == PinType::Exec) continue;
            host.AddDynamicPin(callNode, false, srcPin.name, srcPin.type);
            keepInputs.push_back(srcPin.name);
        }
    }
    std::vector<std::string> toRemoveIn;
    for (const Pin& p : callNode.inputs) {
        if (p.type == PinType::Exec) continue;
        if (std::find(keepInputs.begin(), keepInputs.end(), p.name) == keepInputs.end()) toRemoveIn.push_back(p.name);
    }
    for (const std::string& name : toRemoveIn) host.RemoveDynamicPin(callNode, false, name);

    std::vector<std::string> keepOutputs;
    if (returnNode) {
        for (const Pin& srcPin : returnNode->inputs) {
            if (srcPin.type == PinType::Exec) continue;
            host.AddDynamicPin(callNode, true, srcPin.name, srcPin.type);
            keepOutputs.push_back(srcPin.name);
        }
    }
    std::vector<std::string> toRemoveOut;
    for (const Pin& p : callNode.outputs) {
        if (p.type == PinType::Exec) continue;
        if (std::find(keepOutputs.begin(), keepOutputs.end(), p.name) == keepOutputs.end()) toRemoveOut.push_back(p.name);
    }
    for (const std::string& name : toRemoveOut) host.RemoveDynamicPin(callNode, true, name);
}

void Stargraph::RefreshWildcardPins(Node* node) {
    if (!node) return;

    if (node->type == "Reroute") {
        std::vector<Node*> component;
        std::vector<Node*> pending = { node };
        while (!pending.empty()) {
            Node* current = pending.back();
            pending.pop_back();
            if (std::find(component.begin(), component.end(), current) != component.end()) continue;
            component.push_back(current);
            for (const Link& lk : links) {
                Node* from = FindNodeByPinId(lk.outputPinId);
                Node* to = FindNodeByPinId(lk.inputPinId);
                if (from == current && to && to->type == "Reroute") pending.push_back(to);
                if (to == current && from && from->type == "Reroute") pending.push_back(from);
            }
        }

        const Pin* source = nullptr;
        auto consider = [&](const Pin* candidate) {
            if (source || !candidate) return;
            if (candidate->type == PinType::Wildcard || candidate->type == PinType::None) return;
            if (candidate->type == PinType::Array && (candidate->elementType == PinType::Wildcard || candidate->elementType == PinType::None)) return;
            source = candidate;
        };
        for (Node* member : component) {
            for (const Link& lk : links) {
                for (const Pin& pin : member->inputs) {
                    if (pin.id != lk.inputPinId) continue;
                    Node* from = FindNodeByPinId(lk.outputPinId);
                    if (from && from->type != "Reroute") consider(FindPin(lk.outputPinId));
                }
                for (const Pin& pin : member->outputs) {
                    if (pin.id != lk.outputPinId) continue;
                    Node* to = FindNodeByPinId(lk.inputPinId);
                    if (to && to->type != "Reroute") consider(FindPin(lk.inputPinId));
                }
            }
        }

        for (Node* member : component) {
            for (std::vector<Pin>* side : { &member->inputs, &member->outputs }) {
                for (Pin& pin : *side) {
                    if (source) {
                        pin.type = source->type;
                        pin.elementType = source->elementType;
                        pin.objectKind = source->objectKind;
                        pin.isWildcard = false;
                    } else {
                        pin.type = PinType::Wildcard;
                        pin.elementType = PinType::None;
                        pin.objectKind.clear();
                        pin.isWildcard = true;
                    }
                }
            }
            member->wildcardResolved = false;
        }
        InvalidateCaches();
        return;
    }

    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node->type);
    if (!tmpl || !tmpl->createPins) return;

    Node blank;
    tmpl->createPins(&blank);

    auto findTemplatePin = [](const std::vector<Pin>& templatePins, const std::string& name) -> const Pin* {
        for (const Pin& p : templatePins) if (p.name == name) return &p;
        return nullptr;
    };
    auto isWildcardClass = [&](const std::vector<Pin>& templatePins, const std::string& name, bool isInput) {
        if (const Pin* tp = findTemplatePin(templatePins, name)) return tp->type == PinType::Wildcard;
        return isInput && IsDynamicWildcardInputName(*tmpl, name);
    };
    auto isArrayWildcardClass = [&](const std::vector<Pin>& templatePins, const std::string& name) {
        const Pin* tp = findTemplatePin(templatePins, name);
        return tp && tp->type == PinType::Array && tp->elementType == PinType::Wildcard;
    };
    auto isOpen = [](const Pin& p) {
        return p.type == PinType::Wildcard || (p.type == PinType::Array && p.elementType == PinType::Wildcard);
    };

    bool anyWildcard = false;
    for (Pin& pin : node->inputs) {
        if (isWildcardClass(blank.inputs, pin.name, true)) { pin.type = PinType::Wildcard; pin.objectKind.clear(); anyWildcard = true; }
        else if (isArrayWildcardClass(blank.inputs, pin.name)) { pin.type = PinType::Array; pin.elementType = PinType::Wildcard; anyWildcard = true; }
    }
    for (Pin& pin : node->outputs) {
        if (isWildcardClass(blank.outputs, pin.name, false)) { pin.type = PinType::Wildcard; pin.objectKind.clear(); anyWildcard = true; }
        else if (isArrayWildcardClass(blank.outputs, pin.name)) { pin.type = PinType::Array; pin.elementType = PinType::Wildcard; anyWildcard = true; }
    }
    if (!anyWildcard) return;

    PinType resolved = PinType::None;
    std::string resolvedKind;
    auto consider = [&](const Pin& other) {
        if (resolved != PinType::None) return;
        PinType candidate = other.type == PinType::Array ? other.elementType : other.type;
        if (candidate == PinType::Wildcard) return;
        if (other.type == PinType::Array && candidate == PinType::None) return;
        resolved = candidate;
        resolvedKind = other.objectKind;
    };

    for (const Pin& pin : node->inputs) {
        if (!isOpen(pin)) continue;
        for (const Link& lk : links) {
            if (lk.inputPinId != pin.id) continue;
            if (const Node* fromNode = FindNodeByPinId(lk.outputPinId)) {
                for (const Pin& p : fromNode->outputs) if (p.id == lk.outputPinId) consider(p);
            }
        }
    }
    if (resolved == PinType::None) {
        for (const Pin& pin : node->outputs) {
            if (!isOpen(pin)) continue;
            for (const Link& lk : links) {
                if (lk.outputPinId != pin.id) continue;
                if (const Node* toNode = FindNodeByPinId(lk.inputPinId)) {
                    for (const Pin& p : toNode->inputs) if (p.id == lk.inputPinId) consider(p);
                }
            }
        }
    }
    if (resolved == PinType::None) { InvalidateCaches(); return; }

    auto apply = [&](std::vector<Pin>& pins) {
        for (Pin& pin : pins) {
            if (pin.type == PinType::Wildcard) {
                pin.type = resolved;
                if (resolved == PinType::Object) pin.objectKind = resolvedKind;
            } else if (pin.type == PinType::Array && pin.elementType == PinType::Wildcard) {
                pin.elementType = resolved;
                if (resolved == PinType::Object) pin.objectKind = resolvedKind;
            }
        }
    };
    apply(node->inputs);
    apply(node->outputs);
    InvalidateCaches();
}

Pin* Stargraph::AddDynamicWildcardInput(Node& node) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !HasDynamicWildcardInputs(*tmpl)) return nullptr;
    if (tmpl->dynamicInputsEnabled && !tmpl->dynamicInputsEnabled(node)) return nullptr;

    int count = 0;
    for (const Pin& pin : node.inputs) if (IsDynamicWildcardInputName(*tmpl, pin.name)) count++;
    if (count >= kMaxDynamicWildcardInputs) return nullptr;

    Pin* pin = AddDynamicPin(node, false, DynamicWildcardInputName(*tmpl, count), PinType::Wildcard);
    if (pin) pin->isWildcard = true;
    NormalizeNodePins(node);
    for (Pin& p : node.inputs) if (p.name == DynamicWildcardInputName(*tmpl, count)) return &p;
    return nullptr;
}

bool Stargraph::RemoveDynamicWildcardInput(Node& node, int pinId) {
    const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(node.type);
    if (!tmpl || !HasDynamicWildcardInputs(*tmpl)) return false;
    if (tmpl->dynamicInputsEnabled && !tmpl->dynamicInputsEnabled(node)) return false;

    std::string name;
    int dynamicCount = 0;
    for (const Pin& pin : node.inputs) {
        if (!IsDynamicWildcardInputName(*tmpl, pin.name)) continue;
        dynamicCount++;
        if (pin.id == pinId) name = pin.name;
    }
    if (name.empty()) return false;
    if (tmpl->dynamicWildcardInputsNumeric && (name == "0" || dynamicCount <= 2)) return false;

    RemoveDynamicPin(node, false, name);

    int index = 0;
    for (Pin& pin : node.inputs) {
        if (!IsDynamicWildcardInputName(*tmpl, pin.name)) continue;
        pin.name = DynamicWildcardInputName(*tmpl, index++);
    }
    NormalizeNodePins(node);
    InvalidateCaches();
    return true;
}

void Stargraph::SyncArrayVariables() {
    for (Variable& var : variables) {
        if (var.isArray) var.value = StarArray::Encode(var.arrayValues);
    }
    for (FunctionDef& fn : functions) fn.graph.SyncArrayVariables();
}

void Stargraph::AddVariable(const Variable& var) {
    variables.push_back(var);
    InvalidateCaches();
}

void Stargraph::RemoveVariable(size_t index) {
    if (index < variables.size()) variables.erase(variables.begin() + index);
    InvalidateCaches();
}

void Stargraph::RenameVariable(size_t index, const std::string& newName) {
    if (index < variables.size()) variables[index].name = newName;
    InvalidateCaches();
}

FunctionDef* Stargraph::AddFunction(const std::string& name) {
    functions.emplace_back();
    FunctionDef* fn = &functions.back();
    fn->name = name;
    fn->graph.assetName = name;

    Node* entry = fn->graph.CreateNode("Function_Entry");
    if (entry) { entry->posX = 80; entry->posY = 120; }

    Node* ret = fn->graph.CreateNode("Return");
    if (ret) { ret->posX = 520; ret->posY = 120; }

    return fn;
}

void Stargraph::RemoveFunction(size_t index) {
    if (index < functions.size()) functions.erase(functions.begin() + index);
}

void Stargraph::RenameFunction(size_t index, const std::string& newName) {
    if (index < functions.size()) functions[index].name = newName;
}

FunctionDef* Stargraph::FindFunction(const std::string& name) {
    for (FunctionDef& fn : functions) if (fn.name == name) return &fn;
    return nullptr;
}

const FunctionDef* Stargraph::FindFunction(const std::string& name) const {
    for (const FunctionDef& fn : functions) if (fn.name == name) return &fn;
    return nullptr;
}

void Stargraph::AddComponent(const ComponentDecl& comp) {
    components.push_back(comp);
}

void Stargraph::RemoveComponent(size_t index) {
    if (index < components.size()) components.erase(components.begin() + index);
}

Pin* Stargraph::FindPin(int pinId) {
    for (Node& node : nodes) {
        for (Pin& pin : node.inputs) {
            if (pin.id == pinId) return &pin;
        }
        for (Pin& pin : node.outputs) {
            if (pin.id == pinId) return &pin;
        }
    }
    return nullptr;
}

Node* Stargraph::FindNodeByPinId(int pinId) {
    if (!cachesBuilt) BuildCaches();
    auto it = pinToNodeCache.find(pinId);
    return it != pinToNodeCache.end() ? it->second : nullptr;
}

Node* Stargraph::FindNodeByType(const std::string& type) {
    const std::vector<Node*>& typed = GetNodesOfType(type);
    return typed.empty() ? nullptr : typed[0];
}

Node* Stargraph::FindNodeById(int id) {
    std::string idStr = std::to_string(id);
    for (Node& node : nodes) {
        if (node.id == idStr) return &node;
    }
    return nullptr;
}

static void RegisterActorKindIfAsset(const std::string& kind) {
    if (kind.find('.') != std::string::npos) ObjectKindRegistry::Get().RegisterKind(kind, "Actor");
}

Node* Stargraph::FindCustomEventByName(const std::string& eventName) {
    for (Node* node : GetNodesOfType("Custom_Event")) {
        if (node->title == eventName) return node;
    }
    return nullptr;
}

const std::vector<Node*>& Stargraph::GetNodesOfType(const std::string& type) {
    if (!cachesBuilt) BuildCaches();
    static const std::vector<Node*> empty;
    auto it = nodesByTypeCache.find(type);
    return it != nodesByTypeCache.end() ? it->second : empty;
}

void Stargraph::BuildCaches() {
    nodesByTypeCache.clear();
    pinToNodeCache.clear();
    outputPinToLinksCache.clear();
    inputPinToLinkCache.clear();
    variableByNameCache.clear();

    for (Node& node : nodes) {
        nodesByTypeCache[node.type].push_back(&node);
        for (const Pin& pin : node.inputs) pinToNodeCache[pin.id] = &node;
        for (const Pin& pin : node.outputs) pinToNodeCache[pin.id] = &node;
    }
    for (Link& link : links) {
        outputPinToLinksCache[link.outputPinId].push_back(&link);
        inputPinToLinkCache[link.inputPinId] = &link;
    }
    for (Variable& v : variables) {
        variableByNameCache[v.name] = &v;
    }

    cachesBuilt = true;
}

void Stargraph::InvalidateCaches() {
    cachesBuilt = false;
    nodesByTypeCache.clear();
    pinToNodeCache.clear();
    outputPinToLinksCache.clear();
    inputPinToLinkCache.clear();
    variableByNameCache.clear();
}

Pin* Stargraph::AddDynamicPin(Node& node, bool isOutput, const std::string& name, PinType type) {
    std::vector<Pin>& pins = isOutput ? node.outputs : node.inputs;
    for (Pin& pin : pins) {
        if (pin.name == name) return &pin;
    }
    Pin pin;
    pin.name = name;
    pin.type = type;
    pin.isInput = !isOutput;
    pin.isOutput = isOutput;
    pin.id = nextPinId++;
    pins.push_back(pin);
    return &pins.back();
}

void Stargraph::RemoveDynamicPin(Node& node, bool isOutput, const std::string& pinName) {
    std::vector<Pin>& pins = isOutput ? node.outputs : node.inputs;
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        if (it->name == pinName) {
            RemoveLink(it->id);
            pins.erase(it);
            return;
        }
    }
}

Variable* Stargraph::FindVariable(const std::string& name) {
    if (!cachesBuilt) BuildCaches();
    auto it = variableByNameCache.find(name);
    return it != variableByNameCache.end() ? it->second : nullptr;
}

const Variable* Stargraph::FindVariable(const std::string& name) const {
    for (const Variable& v : variables) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

void Stargraph::CaptureVariableDefaults() {
    for (Variable& v : variables) {
        v.defaultValue = v.value;
    }
    for (FunctionDef& fn : functions) {
        fn.graph.CaptureVariableDefaults();
    }
}

const std::vector<Link*>& Stargraph::GetLinksFromPin(int outputPinId) {
    if (!cachesBuilt) BuildCaches();
    static const std::vector<Link*> empty;
    auto it = outputPinToLinksCache.find(outputPinId);
    return it != outputPinToLinksCache.end() ? it->second : empty;
}

Link* Stargraph::GetLinkToPin(int inputPinId) {
    if (!cachesBuilt) BuildCaches();
    auto it = inputPinToLinkCache.find(inputPinId);
    return it != inputPinToLinkCache.end() ? it->second : nullptr;
}

PinType Stargraph::GetConnectedOutputType(int inputPinId) {
    Link* link = GetLinkToPin(inputPinId);
    if (!link) return PinType::None;
    Node* fromNode = FindNodeByPinId(link->outputPinId);
    if (!fromNode) return PinType::None;
    for (const Pin& p : fromNode->outputs) {
        if (p.id == link->outputPinId) return p.type;
    }
    return PinType::None;
}

bool Stargraph::RemoveNode(int nodeId) {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        if (std::stoi(it->id) == nodeId) {
            for (const Pin& pin : it->inputs) RemoveLink(pin.id);
            for (const Pin& pin : it->outputs) RemoveLink(pin.id);
            nodes.erase(it);
            InvalidateCaches();
            return true;
        }
    }
    return false;
}

std::string PinTypeToString(PinType type) {
    switch (type) {
        case PinType::Exec: return "Exec";
        case PinType::Boolean: return "Boolean";
        case PinType::Integer: return "Integer";
        case PinType::Float: return "Float";
        case PinType::String: return "String";
        case PinType::Vec3: return "Vec3";
        case PinType::Rotator: return "Rotator";
        case PinType::Object: return "Object";
        case PinType::Array: return "Array";
        case PinType::Wildcard: return "Wildcard";
        default: return "None";
    }
}

PinType PinTypeFromString(const std::string& text) {
    if (text == "Exec") return PinType::Exec;
    if (text == "Boolean") return PinType::Boolean;
    if (text == "Integer") return PinType::Integer;
    if (text == "Float") return PinType::Float;
    if (text == "String") return PinType::String;
    if (text == "Vec3") return PinType::Vec3;
    if (text == "Rotator") return PinType::Rotator;
    if (text == "Object") return PinType::Object;
    if (text == "Array") return PinType::Array;
    if (text == "Wildcard") return PinType::Wildcard;
    return PinType::None;
}

static bool FindPinLocation(const std::deque<Node>& nodes, int pinId, std::string& outNodeId, std::string& outPinName) {
    for (const Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) { outNodeId = node.id; outPinName = pin.name; return true; }
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) { outNodeId = node.id; outPinName = pin.name; return true; }
        }
    }
    return false;
}

bool Stargraph::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    return SaveToStream(out);
}

std::string Stargraph::SerializeToString() const {
    std::ostringstream out;
    SaveToStream(out);
    return out.str();
}

bool Stargraph::LoadFromString(const std::string& text) {
    std::istringstream in(text);
    return LoadFromStream(in);
}

static const char* const kStargraphInterfaceName = "Stargraph";

bool Stargraph::SaveToStream(std::ostream& out) const {
    ActorAssetHeader header;
    header.version = formatVersion;
    header.assetName = assetName;
    header.parentClass = parentClass;
    header.programmingInterface = kStargraphInterfaceName;
    ActorAsset::WriteHeader(out, header);
    ActorAsset::WriteSettings(out, actorSettings);
    ActorAsset::WriteComponents(out, components);
    SaveContent(out, ActorAsset::ContentPrefix);
    return true;
}

void Stargraph::SaveContent(std::ostream& out, const std::string& prefix) const {
    SaveGraphContent(out, prefix);

    out << "[" << prefix << "Functions]\n";
    out << "Count=" << functions.size() << "\n";
    for (size_t i = 0; i < functions.size(); i++) {
        out << "Name" << i << "=" << functions[i].name << "\n";
        if (functions[i].pure) out << "Pure" << i << "=true\n";
    }
    out << "\n";

    for (size_t i = 0; i < functions.size(); i++) {
        functions[i].graph.SaveGraphContent(out, prefix + "Function" + std::to_string(i) + ".");
    }
}

void Stargraph::SaveGraphContent(std::ostream& out, const std::string& prefix) const {
    out << "[" << prefix << "Variables]\n";
    out << "Count=" << variables.size() << "\n";
    out << "\n";
    for (size_t i = 0; i < variables.size(); i++) {
        const Variable& var = variables[i];
        out << "[" << prefix << "Variable" << i << "]\n";
        out << "Name=" << var.name << "\n";
        out << "Type=" << PinTypeToString(var.type) << "\n";
        if (var.type == PinType::Object && !var.objectKind.empty()) out << "ObjectKind=" << var.objectKind << "\n";
        out << "IsArray=" << (var.isArray ? "true" : "false") << "\n";
        if (var.isArray) {
            out << "ArrayCount=" << var.arrayValues.size() << "\n";
            for (size_t e = 0; e < var.arrayValues.size(); e++) {
                const std::string& element = var.arrayValues[e];
                if (element.find_first_of("\n\r\\") != std::string::npos) out << "ElementEsc" << e << "=" << EscapeCaseText(element) << "\n";
                else out << "Element" << e << "=" << element << "\n";
            }
        } else {
            out << "Value=" << var.value << "\n";
        }
        out << "Replication=" << ReplicationModeToString(var.replication) << "\n";
        out << "SaveFile=" << var.saveFile << "\n";
        out << "\n";
    }

    out << "[" << prefix << "Nodes]\n";
    out << "Count=" << nodes.size() << "\n";
    out << "\n";
    std::vector<const Node*> sortedNodes;
    for (const Node& node : nodes) sortedNodes.push_back(&node);
    std::sort(sortedNodes.begin(), sortedNodes.end(), [](const Node* a, const Node* b) {
        return SafeParse::ToInt(a->id) < SafeParse::ToInt(b->id);
    });
    for (const Node* nodePtr : sortedNodes) {
        const Node& node = *nodePtr;
        out << "[" << prefix << "Node" << node.id << "]\n";
        out << "Type=" << node.type << "\n";
        if (node.title != node.type) out << "Title=" << node.title << "\n";
        out << "Pos=" << node.posX << "," << node.posY << "\n";
        if (!node.componentClass.empty()) out << "ComponentClass=" << node.componentClass << "\n";
        if (node.type == "Custom_Event") {
            if (node.eventReplication != ReplicationMode::Local)
                out << "Replication=" << ReplicationModeToString(node.eventReplication) << "\n";
            if (node.eventReliable) out << "Reliable=true\n";
        }

        Node templateNode;
        templateNode.type = node.type;
        const NodeTemplate* nodeTmpl = NodeRegistry::Get().GetTemplate(node.type);
        if (nodeTmpl && nodeTmpl->createPins) nodeTmpl->createPins(&templateNode);
        if (nodeTmpl && nodeTmpl->dynamicExecOutputs) {
            out << "ExecCaseCount=" << node.execCases.size() << "\n";
            for (size_t i = 0; i < node.execCases.size(); i++) {
                out << "ExecCase" << i << "=" << EscapeCaseText(node.execCases[i]) << "\n";
            }
        }

        auto hasTemplateMatch = [](const std::vector<Pin>& templatePins, const Pin& pin) {
            for (const Pin& tp : templatePins) if (tp.name == pin.name) return true;
            return false;
        };
        bool hasDynamicPins = false;
        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            if (!hasTemplateMatch(templateNode.inputs, pin)) { hasDynamicPins = true; break; }
        }
        if (!hasDynamicPins) {
            for (const Pin& pin : node.outputs) {
                if (pin.type == PinType::Exec) continue;
                if (!hasTemplateMatch(templateNode.outputs, pin)) { hasDynamicPins = true; break; }
            }
        }
        if (hasDynamicPins) {
            std::string inNames, outNames;
            for (const Pin& pin : node.inputs) {
                if (pin.type == PinType::Exec) continue;
                if (!inNames.empty()) inNames += ",";
                inNames += pin.name;
            }
            for (const Pin& pin : node.outputs) {
                if (pin.type == PinType::Exec) continue;
                if (!outNames.empty()) outNames += ",";
                outNames += pin.name;
            }
            out << "InputPinNames=" << inNames << "\n";
            out << "OutputPinNames=" << outNames << "\n";
        }

        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            const Pin* templatePin = nullptr;
            for (const Pin& tp : templateNode.inputs) if (tp.name == pin.name) { templatePin = &tp; break; }
            if (!pin.defaultValue.empty() && (!templatePin || templatePin->defaultValue != pin.defaultValue))
                out << "Pin." << pin.name << "=" << pin.defaultValue << "\n";
            if (!templatePin || templatePin->type != pin.type)
                out << "PinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
            if (pin.type == PinType::Array && (!templatePin || templatePin->elementType != pin.elementType))
                out << "PinElem." << pin.name << "=" << PinTypeToString(pin.elementType) << "\n";
            bool objectLike = pin.type == PinType::Object || (pin.type == PinType::Array && pin.elementType == PinType::Object);
            if (objectLike && !pin.objectKind.empty() && (!templatePin || templatePin->objectKind != pin.objectKind))
                out << "PinKind." << pin.name << "=" << pin.objectKind << "\n";
        }
        for (const Pin& pin : node.outputs) {
            if (pin.type == PinType::Exec) continue;
            const Pin* templatePin = nullptr;
            for (const Pin& tp : templateNode.outputs) if (tp.name == pin.name) { templatePin = &tp; break; }
            if (!templatePin || templatePin->type != pin.type)
                out << "OutPinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
            if (pin.type == PinType::Array && (!templatePin || templatePin->elementType != pin.elementType))
                out << "OutPinElem." << pin.name << "=" << PinTypeToString(pin.elementType) << "\n";
            bool objectLike = pin.type == PinType::Object || (pin.type == PinType::Array && pin.elementType == PinType::Object);
            if (objectLike && !pin.objectKind.empty() && (!templatePin || templatePin->objectKind != pin.objectKind))
                out << "OutPinKind." << pin.name << "=" << pin.objectKind << "\n";
        }
        out << "\n";
    }

    out << "[" << prefix << "Links]\n";
    struct ResolvedLink {
        std::string id, fromNode, fromPin, toNode, toPin;
    };
    std::vector<ResolvedLink> resolvedLinks;
    for (size_t i = 0; i < links.size(); i++) {
        ResolvedLink resolved;
        resolved.id = links[i].id;
        if (!FindPinLocation(nodes, links[i].outputPinId, resolved.fromNode, resolved.fromPin)) continue;
        if (!FindPinLocation(nodes, links[i].inputPinId, resolved.toNode, resolved.toPin)) continue;
        resolvedLinks.push_back(resolved);
    }
    out << "Count=" << resolvedLinks.size() << "\n";
    out << "\n";
    std::sort(resolvedLinks.begin(), resolvedLinks.end(), [](const ResolvedLink& a, const ResolvedLink& b) {
        return SafeParse::ToInt(a.id) < SafeParse::ToInt(b.id);
    });
    for (const ResolvedLink& resolved : resolvedLinks) {
        out << "[" << prefix << "Link" << resolved.id << "]\n";
        out << "FromNode=" << resolved.fromNode << "\n";
        out << "FromPin=" << resolved.fromPin << "\n";
        out << "ToNode=" << resolved.toNode << "\n";
        out << "ToPin=" << resolved.toPin << "\n";
        out << "\n";
    }
}

using SectionFileIO::ParsedSection;
using SectionFileIO::ParseSections;
using SectionFileIO::FindEntry;

bool Stargraph::LoadFromFile(const std::string& path) {
    std::unique_ptr<std::istream> stream = CookedFile::OpenStream(path);
    if (!stream) return false;
    std::istream& in = *stream;
    return LoadFromStream(in);
}

bool Stargraph::LoadFromStream(std::istream& in) {
    components.clear();
    actorSettings = ActorSettings();
    functions.clear();

    std::vector<ParsedSection> sections = ParseSections(in);

    ActorAssetHeader header = ActorAsset::ReadHeader(sections);
    if (header.found && header.programmingInterface != kStargraphInterfaceName) return false;
    if (header.found) {
        formatVersion = header.version;
        assetName = header.assetName;
        parentClass = header.parentClass;
    }

    actorSettings = ActorAsset::ReadSettings(sections);
    ActorAsset::ReadComponents(sections, components);
    return LoadContent(sections, header.found && !header.isLegacy ? std::string(ActorAsset::ContentPrefix) : std::string());
}

bool Stargraph::LoadContent(const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) {
    functions.clear();

    for (const ParsedSection& section : sections) {
        if (section.name != prefix + "Functions") continue;
        int count = 0;
        if (const std::string* v = FindEntry(section, "Count")) count = SafeParse::ToInt(*v);
        for (int i = 0; i < count; i++) {
            if (const std::string* v = FindEntry(section, "Name" + std::to_string(i))) {
                FunctionDef* fn = AddFunction(*v);
                fn->graph.LoadGraphContent(sections, prefix + "Function" + std::to_string(i) + ".");
                if (const std::string* p = FindEntry(section, "Pure" + std::to_string(i))) fn->pure = (*p == "true");
                if (fn->pure && fn->graph.HasLatentNodes()) {
                    TraceLog(LOG_WARNING, "Stargraph: function \"%s\" is marked pure but contains latent nodes, loaded as impure", fn->name.c_str());
                    fn->pure = false;
                }
            }
        }
    }

    bool loaded = LoadGraphContent(sections, prefix);

    for (const FunctionDef& fn : functions) {
        if (fn.pure) SyncFunctionCallNodes(fn.name);
    }
    return loaded;
}

bool Stargraph::LoadGraphContent(const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) {
    nodes.clear();
    InvalidateCaches();
    links.clear();
    variables.clear();
    nextPinId = 1;

    const std::string nodesHeader     = prefix + "Nodes";
    const std::string nodePrefix      = prefix + "Node";
    const std::string variablesHeader = prefix + "Variables";
    const std::string variablePrefix  = prefix + "Variable";
    const std::string linksHeader     = prefix + "Links";
    const std::string linkPrefix      = prefix + "Link";

    std::vector<Node*> createdNodes;

    for (const ParsedSection& section : sections) {
        if (section.name != nodesHeader && section.name.rfind(nodePrefix, 0) == 0) {
            const std::string* typeValue = FindEntry(section, "Type");
            if (!typeValue) continue;

            Node* node = CreateNode(*typeValue);
            node->id = section.name.substr(nodePrefix.size());
            if (const std::string* v = FindEntry(section, "Title")) node->title = *v;
            if (const std::string* v = FindEntry(section, "Pos")) {
                size_t comma = v->find(',');
                if (comma != std::string::npos) {
                    node->posX = SafeParse::ToFloat(v->substr(0, comma));
                    node->posY = SafeParse::ToFloat(v->substr(comma + 1));
                }
            } else {
                if (const std::string* px = FindEntry(section, "PosX")) node->posX = SafeParse::ToFloat(*px);
                if (const std::string* py = FindEntry(section, "PosY")) node->posY = SafeParse::ToFloat(*py);
            }
            if (const std::string* v = FindEntry(section, "Replication")) node->eventReplication = ReplicationModeFromString(*v);
            if (const std::string* v = FindEntry(section, "Reliable")) node->eventReliable = (*v == "true");
            if (const std::string* v = FindEntry(section, "ComponentClass")) node->componentClass = *v;

            auto ensurePins = [&](const std::string* namesEntry, std::vector<Pin>& pins, bool isOutput) {
                if (!namesEntry || namesEntry->empty()) return;
                size_t start = 0;
                while (start <= namesEntry->size()) {
                    size_t comma = namesEntry->find(',', start);
                    std::string pinName = (comma == std::string::npos) ? namesEntry->substr(start) : namesEntry->substr(start, comma - start);
                    if (!pinName.empty()) {
                        bool exists = false;
                        for (const Pin& p : pins) if (p.name == pinName) { exists = true; break; }
                        if (!exists) {
                            Pin pin;
                            pin.name = pinName;
                            pin.isInput = !isOutput;
                            pin.isOutput = isOutput;
                            pin.id = nextPinId++;
                            pins.push_back(pin);
                        }
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            };
            ensurePins(FindEntry(section, "InputPinNames"), node->inputs, false);
            ensurePins(FindEntry(section, "OutputPinNames"), node->outputs, true);

            for (const auto& entry : section.entries) {
                if (entry.first.rfind("Pin.", 0) == 0) {
                    std::string pinName = ResolveInputPinAlias(node->type, entry.first.substr(4));
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.defaultValue = entry.second; break; }
                    }
                } else if (entry.first.rfind("PinType.", 0) == 0) {
                    std::string pinName = ResolveInputPinAlias(node->type, entry.first.substr(8));
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.type = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("PinElem.", 0) == 0) {
                    std::string pinName = ResolveInputPinAlias(node->type, entry.first.substr(8));
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.elementType = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("OutPinElem.", 0) == 0) {
                    std::string pinName = entry.first.substr(11);
                    for (Pin& pin : node->outputs) {
                        if (pin.name == pinName) { pin.elementType = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("OutPinType.", 0) == 0) {
                    std::string pinName = entry.first.substr(11);
                    for (Pin& pin : node->outputs) {
                        if (pin.name == pinName) { pin.type = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("PinKind.", 0) == 0) {
                    std::string pinName = ResolveInputPinAlias(node->type, entry.first.substr(8));
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.objectKind = entry.second; break; }
                    }
                    RegisterActorKindIfAsset(entry.second);
                } else if (entry.first.rfind("OutPinKind.", 0) == 0) {
                    std::string pinName = entry.first.substr(11);
                    for (Pin& pin : node->outputs) {
                        if (pin.name == pinName) { pin.objectKind = entry.second; break; }
                    }
                    RegisterActorKindIfAsset(entry.second);
                }
            }

            NormalizeNodePins(*node);
            for (Pin& pin : node->inputs)  if (pin.type == PinType::Wildcard) pin.isWildcard = true;
            for (Pin& pin : node->outputs) if (pin.type == PinType::Wildcard) pin.isWildcard = true;

            if (const NodeTemplate* execTmpl = NodeRegistry::Get().GetTemplate(node->type); execTmpl && execTmpl->dynamicExecOutputs) {
                if (const std::string* countText = FindEntry(section, "ExecCaseCount")) {
                    int caseCount = SafeParse::ToInt(*countText);
                    node->execCases.clear();
                    for (int i = 0; i < caseCount; i++) {
                        const std::string* caseText = FindEntry(section, "ExecCase" + std::to_string(i));
                        node->execCases.push_back(caseText ? UnescapeCaseText(*caseText) : std::string());
                    }
                    RebuildDynamicExecOutputs(*node);
                } else if (!execTmpl->dynamicExecLegacyCases.empty()) {
                    node->execCases = execTmpl->dynamicExecLegacyCases;
                    RebuildDynamicExecOutputs(*node);
                }
            }

            createdNodes.push_back(node);
        } else if (section.name != variablesHeader && section.name.rfind(variablePrefix, 0) == 0) {
            Variable var;
            if (const std::string* v = FindEntry(section, "Name")) var.name = *v;
            if (const std::string* v = FindEntry(section, "Type")) var.type = PinTypeFromString(*v);
            if (const std::string* v = FindEntry(section, "ObjectKind")) { var.objectKind = *v; RegisterActorKindIfAsset(*v); }
            if (const std::string* v = FindEntry(section, "IsArray")) var.isArray = (*v == "true");
            if (var.isArray) {
                int count = 0;
                if (const std::string* v = FindEntry(section, "ArrayCount")) count = SafeParse::ToInt(*v);
                var.arrayValues.reserve(count > 0 ? (size_t)count : 0);
                for (int e = 0; e < count; e++) {
                    if (const std::string* v = FindEntry(section, "ElementEsc" + std::to_string(e))) var.arrayValues.push_back(UnescapeCaseText(*v));
                    else if (const std::string* v = FindEntry(section, "Element" + std::to_string(e))) var.arrayValues.push_back(*v);
                    else var.arrayValues.push_back(var.DefaultElementText());
                }
                var.value = StarArray::Encode(var.arrayValues);
            } else if (const std::string* v = FindEntry(section, "Value")) {
                var.value = *v;
            }
            if (const std::string* v = FindEntry(section, "Replication")) var.replication = ReplicationModeFromString(*v);
            if (const std::string* v = FindEntry(section, "SaveFile")) var.saveFile = *v;
            variables.push_back(var);
        }
    }

    int maxLoadedId = 0;
    for (const Node* node : createdNodes) {
        int idValue = SafeParse::ToInt(node->id);
        if (idValue > maxLoadedId) maxLoadedId = idValue;
    }
    nextNodeId = maxLoadedId + 1;

    for (const ParsedSection& section : sections) {
        if (section.name == linksHeader || section.name.rfind(linkPrefix, 0) != 0) continue;

        const std::string* fromNodeStr = FindEntry(section, "FromNode");
        const std::string* fromPinStr = FindEntry(section, "FromPin");
        const std::string* toNodeStr = FindEntry(section, "ToNode");
        const std::string* toPinStr = FindEntry(section, "ToPin");
        if (!fromNodeStr || !fromPinStr || !toNodeStr || !toPinStr) continue;

        std::string linkId = section.name.substr(linkPrefix.size());

        Node* fromNode = nullptr;
        Node* toNode = nullptr;
        for (Node* candidate : createdNodes) {
            if (candidate->id == *fromNodeStr) fromNode = candidate;
            if (candidate->id == *toNodeStr) toNode = candidate;
        }
        if (!fromNode || !toNode) continue;

        int outputPinId = 0;
        const std::string resolvedFromPin = ResolveOutputPinAlias(fromNode->type, *fromPinStr);
        for (const Pin& pin : fromNode->outputs) {
            if (pin.name == resolvedFromPin) { outputPinId = pin.id; break; }
        }
        int inputPinId = 0;
        const std::string resolvedToPin = ResolveInputPinAlias(toNode->type, *toPinStr);
        for (const Pin& pin : toNode->inputs) {
            if (pin.name == resolvedToPin) { inputPinId = pin.id; break; }
        }

        if (outputPinId != 0 && inputPinId != 0) {
            AddLink(Link{ outputPinId, inputPinId, linkId });
        }
    }

    int maxLoadedLinkId = 0;
    for (const Link& link : links) {
        int idValue = SafeParse::ToInt(link.id);
        if (idValue > maxLoadedLinkId) maxLoadedLinkId = idValue;
    }
    nextLinkId = maxLoadedLinkId + 1;

    for (Node* created : createdNodes) {
        if (created->type == "Reroute") RefreshWildcardPins(created);
    }

    return true;
}
