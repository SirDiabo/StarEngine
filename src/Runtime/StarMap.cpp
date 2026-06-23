#include "StarMap.h"
#include <fstream>
#include <sstream>
#include <vector>

bool StarMap::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarMap]\n";
    out << "Version=" << formatVersion << "\n";
    out << "Name=" << mapName << "\n";
    out << "\n";

    out << "[Instances]\n";
    out << "Count=" << instances.size() << "\n";
    out << "\n";

    for (size_t i = 0; i < instances.size(); i++) {
        const ActorInstance& inst = instances[i];
        out << "[Instance" << i << "]\n";
        out << "Name=" << inst.name << "\n";
        out << "Class=" << inst.className << "\n";
        out << "Graph=" << inst.graphAssetPath << "\n";
        out << "PosX=" << inst.posX << "\n";
        out << "PosY=" << inst.posY << "\n";
        out << "PosZ=" << inst.posZ << "\n";
        out << "RotX=" << inst.rotX << "\n";
        out << "RotY=" << inst.rotY << "\n";
        out << "RotZ=" << inst.rotZ << "\n";
        out << "ScaleX=" << inst.scaleX << "\n";
        out << "ScaleY=" << inst.scaleY << "\n";
        out << "ScaleZ=" << inst.scaleZ << "\n";
        out << "\n";
    }

    return true;
}

namespace {
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };

    std::vector<Section> ParseFile(std::ifstream& in) {
        std::vector<Section> sections;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (line.front() == '[' && line.back() == ']') {
                sections.push_back({line.substr(1, line.size() - 2), {}});
                continue;
            }
            if (sections.empty()) continue;
            size_t sep = line.find('=');
            if (sep == std::string::npos) continue;
            sections.back().entries.push_back({line.substr(0, sep), line.substr(sep + 1)});
        }
        return sections;
    }

    const std::string* Find(const Section& s, const std::string& key) {
        for (const auto& e : s.entries) if (e.first == key) return &e.second;
        return nullptr;
    }
}

bool StarMap::LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    instances.clear();
    auto sections = ParseFile(in);

    for (const Section& section : sections) {
        if (section.name == "StarMap") {
            if (const std::string* v = Find(section, "Name")) mapName = *v;
            if (const std::string* v = Find(section, "Version")) formatVersion = std::stoi(*v);
        } else if (section.name != "Instances" && section.name.rfind("Instance", 0) == 0) {
            ActorInstance inst;
            if (const std::string* v = Find(section, "Name"))   inst.name  = *v;
            if (const std::string* v = Find(section, "Class"))  inst.className = *v;
            if (const std::string* v = Find(section, "Graph"))  inst.graphAssetPath = *v;
            if (const std::string* v = Find(section, "PosX"))   inst.posX   = std::stof(*v);
            if (const std::string* v = Find(section, "PosY"))   inst.posY   = std::stof(*v);
            if (const std::string* v = Find(section, "PosZ"))   inst.posZ   = std::stof(*v);
            if (const std::string* v = Find(section, "RotX"))   inst.rotX   = std::stof(*v);
            if (const std::string* v = Find(section, "RotY"))   inst.rotY   = std::stof(*v);
            if (const std::string* v = Find(section, "RotZ"))   inst.rotZ   = std::stof(*v);
            if (const std::string* v = Find(section, "ScaleX")) inst.scaleX = std::stof(*v);
            if (const std::string* v = Find(section, "ScaleY")) inst.scaleY = std::stof(*v);
            if (const std::string* v = Find(section, "ScaleZ")) inst.scaleZ = std::stof(*v);
            instances.push_back(inst);
        }
    }

    return true;
}

void StarMap::AddInstance(const ActorInstance& instance) {
    instances.push_back(instance);
}

void StarMap::RemoveInstance(size_t index) {
    if (index < instances.size()) instances.erase(instances.begin() + index);
}
