#include "Stargraph.h"
#include "NodeRegistry.h"
#include <fstream>

Node* Stargraph::CreateNode(const std::string& type) {
    nodes.emplace_back();
    Node* node = &nodes.back();
    node->id = std::to_string(nextNodeId++);
    node->type = type;
    node->title = type;

    const NodeTemplate* nodeTemplate = NodeRegistry::Get().GetTemplate(type);
    if (nodeTemplate && nodeTemplate->createPins) {
        nodeTemplate->createPins(node);
    }

    for (Pin& pin : node->inputs) pin.id = nextPinId++;
    for (Pin& pin : node->outputs) pin.id = nextPinId++;

    return node;
}

void Stargraph::AddLink(const Link& link) {
    links.push_back(link);
}

void Stargraph::RemoveLink(int pinId) {
    for (auto it = links.begin(); it != links.end();) {
        if (it->inputPinId == pinId || it->outputPinId == pinId) {
            it = links.erase(it);
        } else {
            ++it;
        }
    }
}

void Stargraph::AddVariable(const Variable& var) {
    variables.push_back(var);
}

void Stargraph::RemoveVariable(size_t index) {
    if (index < variables.size()) variables.erase(variables.begin() + index);
}

void Stargraph::RenameVariable(size_t index, const std::string& newName) {
    if (index < variables.size()) variables[index].name = newName;
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
    for (Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) return &node;
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) return &node;
        }
    }
    return nullptr;
}

Node* Stargraph::FindNodeByType(const std::string& type) {
    for (Node& node : nodes) {
        if (node.type == type) return &node;
    }
    return nullptr;
}

bool Stargraph::RemoveNode(int nodeId) {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        if (std::stoi(it->id) == nodeId) {
            for (const Pin& pin : it->inputs) RemoveLink(pin.id);
            for (const Pin& pin : it->outputs) RemoveLink(pin.id);
            nodes.erase(it);
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

static bool FindPinLocation(const std::deque<Node>& nodes, int pinId, int& outNodeIndex, std::string& outPinName) {
    for (size_t i = 0; i < nodes.size(); i++) {
        for (const Pin& pin : nodes[i].inputs) {
            if (pin.id == pinId) { outNodeIndex = (int)i; outPinName = pin.name; return true; }
        }
        for (const Pin& pin : nodes[i].outputs) {
            if (pin.id == pinId) { outNodeIndex = (int)i; outPinName = pin.name; return true; }
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
        out << "[Component" << i << "]\n";
        out << "Type=" << comp.componentType << "\n";
        out << "Name=" << comp.name << "\n";
        out << "PosX=" << comp.posX << "\n";
        out << "PosY=" << comp.posY << "\n";
        out << "PosZ=" << comp.posZ << "\n";
        out << "RotX=" << comp.rotX << "\n";
        out << "RotY=" << comp.rotY << "\n";
        out << "RotZ=" << comp.rotZ << "\n";
        out << "ScaleX=" << comp.scaleX << "\n";
        out << "ScaleY=" << comp.scaleY << "\n";
        out << "ScaleZ=" << comp.scaleZ << "\n";
        out << "Mesh=" << comp.meshAssetPath << "\n";
        out << "Material=" << comp.materialAssetPath << "\n";
        out << "CollisionShape=" << comp.collisionShape << "\n";
        out << "CollisionRadius=" << comp.collisionRadius << "\n";
        out << "CollisionHeight=" << comp.collisionHeight << "\n";
        out << "Sound=" << comp.soundAssetPath << "\n";
        out << "AutoPlay=" << (comp.autoPlay ? "true" : "false") << "\n";
        out << "Looping=" << (comp.looping ? "true" : "false") << "\n";
        out << "Volume=" << comp.volume << "\n";
        out << "\n";
    }

    out << "[Variables]\n";
    out << "Count=" << variables.size() << "\n";
    out << "\n";
    for (size_t i = 0; i < variables.size(); i++) {
        out << "[Variable" << i << "]\n";
        out << "Name=" << variables[i].name << "\n";
        out << "Type=" << PinTypeToString(variables[i].type) << "\n";
        out << "Value=" << variables[i].value << "\n";
        out << "\n";
    }

    out << "[Nodes]\n";
    out << "Count=" << nodes.size() << "\n";
    out << "\n";
    for (size_t i = 0; i < nodes.size(); i++) {
        const Node& node = nodes[i];
        out << "[Node" << i << "]\n";
        out << "Type=" << node.type << "\n";
        out << "Title=" << node.title << "\n";
        out << "PosX=" << node.posX << "\n";
        out << "PosY=" << node.posY << "\n";
        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            out << "Pin." << pin.name << "=" << pin.defaultValue << "\n";
            out << "PinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
        }
        for (const Pin& pin : node.outputs) {
            if (pin.type == PinType::Exec) continue;
            out << "OutPinType." << pin.name << "=" << PinTypeToString(pin.type) << "\n";
        }
        out << "\n";
    }

    out << "[Links]\n";
    out << "Count=" << links.size() << "\n";
    out << "\n";
    for (size_t i = 0; i < links.size(); i++) {
        int fromIndex = 0, toIndex = 0;
        std::string fromPin, toPin;
        if (!FindPinLocation(nodes, links[i].outputPinId, fromIndex, fromPin)) continue;
        if (!FindPinLocation(nodes, links[i].inputPinId, toIndex, toPin)) continue;

        out << "[Link" << i << "]\n";
        out << "FromNode=" << fromIndex << "\n";
        out << "FromPin=" << fromPin << "\n";
        out << "ToNode=" << toIndex << "\n";
        out << "ToPin=" << toPin << "\n";
        out << "\n";
    }

    return true;
}

namespace {
    struct ParsedSection {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };

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
    std::ifstream in(path);
    if (!in.is_open()) return false;

    nodes.clear();
    links.clear();
    variables.clear();
    components.clear();
    nextPinId = 1;

    std::vector<ParsedSection> sections = ParseSections(in);
    std::vector<Node*> createdNodes;

    for (const ParsedSection& section : sections) {
        if (section.name == "Stargraph") {
            if (const std::string* v = FindEntry(section, "Version")) formatVersion = std::stoi(*v);
            if (const std::string* v = FindEntry(section, "Name")) assetName = *v;
            if (const std::string* v = FindEntry(section, "ParentClass")) parentClass = *v;
        } else if (section.name != "Components" && section.name.rfind("Component", 0) == 0) {
            ComponentDecl comp;
            if (const std::string* v = FindEntry(section, "Type")) comp.componentType = *v;
            if (const std::string* v = FindEntry(section, "Name")) comp.name = *v;
            if (const std::string* v = FindEntry(section, "PosX")) comp.posX = std::stof(*v);
            if (const std::string* v = FindEntry(section, "PosY")) comp.posY = std::stof(*v);
            if (const std::string* v = FindEntry(section, "PosZ")) comp.posZ = std::stof(*v);
            if (const std::string* v = FindEntry(section, "RotX")) comp.rotX = std::stof(*v);
            if (const std::string* v = FindEntry(section, "RotY")) comp.rotY = std::stof(*v);
            if (const std::string* v = FindEntry(section, "RotZ")) comp.rotZ = std::stof(*v);
            if (const std::string* v = FindEntry(section, "ScaleX")) comp.scaleX = std::stof(*v);
            if (const std::string* v = FindEntry(section, "ScaleY")) comp.scaleY = std::stof(*v);
            if (const std::string* v = FindEntry(section, "ScaleZ")) comp.scaleZ = std::stof(*v);
            if (const std::string* v = FindEntry(section, "Mesh")) comp.meshAssetPath = *v;
            if (const std::string* v = FindEntry(section, "Material")) comp.materialAssetPath = *v;
            if (const std::string* v = FindEntry(section, "CollisionShape")) comp.collisionShape = *v;
            if (const std::string* v = FindEntry(section, "CollisionRadius")) comp.collisionRadius = std::stof(*v);
            if (const std::string* v = FindEntry(section, "CollisionHeight")) comp.collisionHeight = std::stof(*v);
            if (const std::string* v = FindEntry(section, "Sound")) comp.soundAssetPath = *v;
            if (const std::string* v = FindEntry(section, "AutoPlay")) comp.autoPlay = (*v == "true");
            if (const std::string* v = FindEntry(section, "Looping")) comp.looping = (*v == "true");
            if (const std::string* v = FindEntry(section, "Volume")) comp.volume = std::stof(*v);
            components.push_back(comp);
        } else if (section.name != "Nodes" && section.name.rfind("Node", 0) == 0) {
            const std::string* typeValue = FindEntry(section, "Type");
            if (!typeValue) continue;

            Node* node = CreateNode(*typeValue);
            if (const std::string* v = FindEntry(section, "Title")) node->title = *v;
            if (const std::string* v = FindEntry(section, "PosX")) node->posX = std::stof(*v);
            if (const std::string* v = FindEntry(section, "PosY")) node->posY = std::stof(*v);

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
                }
            }

            createdNodes.push_back(node);
        } else if (section.name != "Variables" && section.name.rfind("Variable", 0) == 0) {
            Variable var;
            if (const std::string* v = FindEntry(section, "Name")) var.name = *v;
            if (const std::string* v = FindEntry(section, "Type")) var.type = PinTypeFromString(*v);
            if (const std::string* v = FindEntry(section, "Value")) var.value = *v;
            variables.push_back(var);
        }
    }

    for (const ParsedSection& section : sections) {
        if (section.name == "Links" || section.name.rfind("Link", 0) != 0) continue;

        const std::string* fromNodeStr = FindEntry(section, "FromNode");
        const std::string* fromPinStr = FindEntry(section, "FromPin");
        const std::string* toNodeStr = FindEntry(section, "ToNode");
        const std::string* toPinStr = FindEntry(section, "ToPin");
        if (!fromNodeStr || !fromPinStr || !toNodeStr || !toPinStr) continue;

        int fromIndex = std::stoi(*fromNodeStr);
        int toIndex = std::stoi(*toNodeStr);
        if (fromIndex < 0 || fromIndex >= (int)createdNodes.size()) continue;
        if (toIndex < 0 || toIndex >= (int)createdNodes.size()) continue;

        int outputPinId = 0;
        for (const Pin& pin : createdNodes[fromIndex]->outputs) {
            if (pin.name == *fromPinStr) { outputPinId = pin.id; break; }
        }
        int inputPinId = 0;
        for (const Pin& pin : createdNodes[toIndex]->inputs) {
            if (pin.name == *toPinStr) { inputPinId = pin.id; break; }
        }

        if (outputPinId != 0 && inputPinId != 0) {
            AddLink(Link{ outputPinId, inputPinId });
        }
    }

    return true;
}