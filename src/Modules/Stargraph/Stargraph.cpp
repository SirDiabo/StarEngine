#include "Stargraph.h"
#include "NodeRegistry.h"
#include "ComponentFields.h"
#include "Core/CookedFile.h"
#include "Core/SafeParse.h"
#include "Core/ObjectKindRegistry.h"
#include <fstream>
#include <sstream>
#include <algorithm>

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

std::string Variable::DefaultElementText() const {
    switch (type) {
        case PinType::Boolean: return "false";
        case PinType::Integer: return "0";
        case PinType::Float:   return "0.00";
        case PinType::Vec3:    return "0.00,0.00,0.00";
        default:                return "";
    }
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
        case PinType::Object: return "Object";
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
    if (text == "Object") return PinType::Object;
    return PinType::None;
}

std::string ReplicationModeToString(ReplicationMode mode) {
    switch (mode) {
        case ReplicationMode::Replicated: return "Replicated";
        case ReplicationMode::ServerOnly: return "ServerOnly";
        default: return "Local";
    }
}

ReplicationMode ReplicationModeFromString(const std::string& text) {
    if (text == "Replicated") return ReplicationMode::Replicated;
    if (text == "ServerOnly") return ReplicationMode::ServerOnly;
    return ReplicationMode::Local;
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

    out << "[Stargraph]\n";
    out << "Version=" << formatVersion << "\n";
    out << "Name=" << assetName << "\n";
    out << "ParentClass=" << parentClass << "\n";
    out << "\n";

    out << "[Components]\n";
    out << "Count=" << components.size() << "\n";
    out << "\n";
    for (size_t i = 0; i < components.size(); i++) {
        const ComponentDecl& comp = components[i];
        out << "[Component" << comp.name << "]\n";
        out << "Type=" << comp.componentType << "\n";
        out << "Name=" << comp.name << "\n";
        out << "Parent=" << comp.parentName << "\n";
        if (comp.posX != 0.0f) out << "PosX=" << comp.posX << "\n";
        if (comp.posY != 0.0f) out << "PosY=" << comp.posY << "\n";
        if (comp.posZ != 0.0f) out << "PosZ=" << comp.posZ << "\n";
        if (comp.rotX != 0.0f) out << "RotX=" << comp.rotX << "\n";
        if (comp.rotY != 0.0f) out << "RotY=" << comp.rotY << "\n";
        if (comp.rotZ != 0.0f) out << "RotZ=" << comp.rotZ << "\n";
        if (comp.scaleX != 1.0f) out << "ScaleX=" << comp.scaleX << "\n";
        if (comp.scaleY != 1.0f) out << "ScaleY=" << comp.scaleY << "\n";
        if (comp.scaleZ != 1.0f) out << "ScaleZ=" << comp.scaleZ << "\n";
        for (const ComponentFieldDesc& f : GetComponentFields()) {
            if (f.componentType != comp.componentType) continue;
            switch (f.type) {
                case ComponentFieldType::String:
                    out << f.key << "=" << (comp.*f.stringField) << "\n";
                    break;
                case ComponentFieldType::Float:
                    out << f.key << "=" << (comp.*f.floatField) << "\n";
                    break;
                case ComponentFieldType::Bool:
                    out << f.key << "=" << ((comp.*f.boolField) ? "true" : "false") << "\n";
                    break;
                case ComponentFieldType::StringArray: {
                    const auto& arr = comp.*f.stringArrayField;
                    out << f.key << "Count=" << arr.size() << "\n";
                    for (size_t m = 0; m < arr.size(); m++) out << f.key << m << "=" << arr[m] << "\n";
                    break;
                }
            }
        }
        {
            std::string savable;
            for (const auto& kv : comp.savableFields) {
                if (kv.second.empty()) continue;
                if (!savable.empty()) savable += ",";
                savable += kv.first + ":" + kv.second;
            }
            out << "SavableFields=" << savable << "\n";
        }
        out << "\n";
    }

    SaveGraphContent(out, "");

    out << "[Functions]\n";
    out << "Count=" << functions.size() << "\n";
    for (size_t i = 0; i < functions.size(); i++) {
        out << "Name" << i << "=" << functions[i].name << "\n";
    }
    out << "\n";

    for (size_t i = 0; i < functions.size(); i++) {
        functions[i].graph.SaveGraphContent(out, "Function" + std::to_string(i) + ".");
    }

    return true;
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
                out << "Element" << e << "=" << var.arrayValues[e] << "\n";
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
        if (node.type == "Custom_Event") {
            if (node.eventReplication != ReplicationMode::Local)
                out << "Replication=" << ReplicationModeToString(node.eventReplication) << "\n";
            if (node.eventReliable) out << "Reliable=true\n";
        }

        Node templateNode;
        templateNode.type = node.type;
        const NodeTemplate* nodeTmpl = NodeRegistry::Get().GetTemplate(node.type);
        if (nodeTmpl && nodeTmpl->createPins) nodeTmpl->createPins(&templateNode);

        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            const Pin* templatePin = nullptr;
            for (const Pin& tp : templateNode.inputs) if (tp.name == pin.name) { templatePin = &tp; break; }
            if (!pin.defaultValue.empty() && (!templatePin || templatePin->defaultValue != pin.defaultValue))
                out << "Pin." << pin.name << "=" << pin.defaultValue << "\n";
            if (!templatePin || templatePin->type != pin.type)
                out << "PinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
            if (pin.type == PinType::Object && !pin.objectKind.empty() && (!templatePin || templatePin->objectKind != pin.objectKind))
                out << "PinKind." << pin.name << "=" << pin.objectKind << "\n";
        }
        for (const Pin& pin : node.outputs) {
            if (pin.type == PinType::Exec) continue;
            const Pin* templatePin = nullptr;
            for (const Pin& tp : templateNode.outputs) if (tp.name == pin.name) { templatePin = &tp; break; }
            if (!templatePin || templatePin->type != pin.type)
                out << "OutPinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
            if (pin.type == PinType::Object && !pin.objectKind.empty() && (!templatePin || templatePin->objectKind != pin.objectKind))
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

namespace {
    using StargraphIO::ParsedSection;

    std::vector<ParsedSection> ParseSections(std::ifstream& in) {
        std::vector<ParsedSection> sections;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            if (line.front() == '[' && line.back() == ']') {
                ParsedSection section;
                section.name = line.substr(1, line.size() - 2);
                sections.push_back(section);
                continue;
            }

            if (sections.empty()) continue;

            size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            sections.back().entries.emplace_back(key, value);
        }
        return sections;
    }

    const std::string* FindEntry(const ParsedSection& section, const std::string& key) {
        for (const auto& entry : section.entries) {
            if (entry.first == key) return &entry.second;
        }
        return nullptr;
    }
}

bool Stargraph::LoadFromFile(const std::string& path) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;

    components.clear();
    functions.clear();

    std::vector<ParsedSection> sections = ParseSections(in);

    for (const ParsedSection& section : sections) {
        if (section.name == "Stargraph") {
            if (const std::string* v = FindEntry(section, "Version")) formatVersion = SafeParse::ToInt(*v);
            if (const std::string* v = FindEntry(section, "Name")) assetName = *v;
            if (const std::string* v = FindEntry(section, "ParentClass")) parentClass = *v;
        } else if (section.name != "Components" && section.name.rfind("Component", 0) == 0) {
            ComponentDecl comp;
            if (const std::string* v = FindEntry(section, "Type")) comp.componentType = *v;
            if (const std::string* v = FindEntry(section, "Name")) comp.name = *v;
            if (const std::string* v = FindEntry(section, "Parent")) comp.parentName = *v;
            if (const std::string* v = FindEntry(section, "PosX")) comp.posX = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "PosY")) comp.posY = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "PosZ")) comp.posZ = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "RotX")) comp.rotX = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "RotY")) comp.rotY = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "RotZ")) comp.rotZ = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "ScaleX")) comp.scaleX = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "ScaleY")) comp.scaleY = SafeParse::ToFloat(*v);
            if (const std::string* v = FindEntry(section, "ScaleZ")) comp.scaleZ = SafeParse::ToFloat(*v);
            for (const ComponentFieldDesc& f : GetComponentFields()) {
                if (f.componentType != comp.componentType) continue;
                switch (f.type) {
                    case ComponentFieldType::String:
                        if (const std::string* v = FindEntry(section, f.key)) comp.*f.stringField = *v;
                        break;
                    case ComponentFieldType::Float:
                        if (const std::string* v = FindEntry(section, f.key)) comp.*f.floatField = SafeParse::ToFloat(*v);
                        break;
                    case ComponentFieldType::Bool:
                        if (const std::string* v = FindEntry(section, f.key)) comp.*f.boolField = (*v == "true");
                        break;
                    case ComponentFieldType::StringArray: {
                        if (const std::string* v = FindEntry(section, std::string(f.key) + "Count")) {
                            int count = SafeParse::ToInt(*v);
                            auto& arr = comp.*f.stringArrayField;
                            if (count > 0) arr.resize((size_t)count);
                            for (int m = 0; m < count; m++) {
                                if (const std::string* mv = FindEntry(section, f.key + std::to_string(m)))
                                    arr[(size_t)m] = *mv;
                            }
                        }
                        break;
                    }
                }
            }
            if (const std::string* v = FindEntry(section, "SavableFields")) {
                std::stringstream ss(*v);
                std::string entry;
                while (std::getline(ss, entry, ',')) {
                    size_t colon = entry.find(':');
                    if (colon == std::string::npos) continue;
                    comp.savableFields[entry.substr(0, colon)] = entry.substr(colon + 1);
                }
            }
            components.push_back(comp);
        } else if (section.name == "Functions") {
            int count = 0;
            if (const std::string* v = FindEntry(section, "Count")) count = SafeParse::ToInt(*v);
            for (int i = 0; i < count; i++) {
                if (const std::string* v = FindEntry(section, "Name" + std::to_string(i))) {
                    FunctionDef* fn = AddFunction(*v);
                    fn->graph.LoadGraphContent(sections, "Function" + std::to_string(i) + ".");
                }
            }
        }
    }

    LoadGraphContent(sections, "");

    return true;
}

bool Stargraph::LoadGraphContent(const std::vector<StargraphIO::ParsedSection>& sections, const std::string& prefix) {
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
                    std::string pinName = entry.first.substr(4);
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.defaultValue = entry.second; break; }
                    }
                } else if (entry.first.rfind("PinType.", 0) == 0) {
                    std::string pinName = entry.first.substr(8);
                    for (Pin& pin : node->inputs) {
                        if (pin.name == pinName) { pin.type = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("OutPinType.", 0) == 0) {
                    std::string pinName = entry.first.substr(11);
                    for (Pin& pin : node->outputs) {
                        if (pin.name == pinName) { pin.type = PinTypeFromString(entry.second); break; }
                    }
                } else if (entry.first.rfind("PinKind.", 0) == 0) {
                    std::string pinName = entry.first.substr(8);
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
                    std::string key = "Element" + std::to_string(e);
                    if (const std::string* v = FindEntry(section, key)) var.arrayValues.push_back(*v);
                    else var.arrayValues.push_back(var.DefaultElementText());
                }
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
        for (const Pin& pin : fromNode->outputs) {
            if (pin.name == *fromPinStr) { outputPinId = pin.id; break; }
        }
        int inputPinId = 0;
        for (const Pin& pin : toNode->inputs) {
            if (pin.name == *toPinStr) { inputPinId = pin.id; break; }
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

    return true;
}
