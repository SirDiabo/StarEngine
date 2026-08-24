#include "StarMap.h"
#include "Core/CookedFile.h"
#include "Core/SafeParse.h"
#include "Modules/Stargraph/ComponentFields.h"
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
        out << "Mesh=" << inst.meshAssetPath << "\n";
        out << "CastsShadow=" << (inst.castsShadow ? "true" : "false") << "\n";
        out << "MaterialCount=" << inst.materialAssetPaths.size() << "\n";
        for (size_t m = 0; m < inst.materialAssetPaths.size(); m++) {
            out << "Material" << m << "=" << inst.materialAssetPaths[m] << "\n";
        }
        out << "HasComponent=" << (inst.hasComponent ? "true" : "false") << "\n";
        out << "ActorKind=" << inst.actorKind << "\n";
        if (inst.hasComponent) {
            out << "ComponentType=" << inst.component.componentType << "\n";
            for (const ComponentFieldDesc& fd : GetComponentFields()) {
                if (fd.componentType != inst.component.componentType) continue;
                std::string key = std::string("Comp.") + fd.key;
                switch (fd.type) {
                    case ComponentFieldType::String:
                        out << key << "=" << inst.component.*(fd.stringField) << "\n";
                        break;
                    case ComponentFieldType::Float:
                        out << key << "=" << inst.component.*(fd.floatField) << "\n";
                        break;
                    case ComponentFieldType::Bool:
                        out << key << "=" << ((inst.component.*(fd.boolField)) ? "true" : "false") << "\n";
                        break;
                    case ComponentFieldType::StringArray: {
                        const std::vector<std::string>& arr = inst.component.*(fd.stringArrayField);
                        out << key << ".Count=" << arr.size() << "\n";
                        for (size_t a = 0; a < arr.size(); a++)
                            out << key << "." << a << "=" << arr[a] << "\n";
                        break;
                    }
                }
            }
        }
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
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;

    instances.clear();
    auto sections = ParseFile(in);

    for (const Section& section : sections) {
        if (section.name == "StarMap") {
            if (const std::string* v = Find(section, "Name")) mapName = *v;
            if (const std::string* v = Find(section, "Version")) formatVersion = SafeParse::ToInt(*v);
        } else if (section.name != "Instances" && section.name.rfind("Instance", 0) == 0) {
            ActorInstance inst;
            if (const std::string* v = Find(section, "Name"))   inst.name  = *v;
            if (const std::string* v = Find(section, "Class"))  inst.className = *v;
            if (const std::string* v = Find(section, "Graph"))  inst.graphAssetPath = *v;
            if (const std::string* v = Find(section, "Mesh"))   inst.meshAssetPath = *v;
            if (const std::string* v = Find(section, "CastsShadow")) inst.castsShadow = (*v == "true");
            if (const std::string* v = Find(section, "MaterialCount")) {
                int count = SafeParse::ToInt(*v);
                if (count > 0) inst.materialAssetPaths.resize((size_t)count);
                for (int m = 0; m < count; m++) {
                    if (const std::string* mv = Find(section, "Material" + std::to_string(m)))
                        inst.materialAssetPaths[m] = *mv;
                }
            }
            if (const std::string* v = Find(section, "HasComponent")) inst.hasComponent = (*v == "true");
            if (const std::string* v = Find(section, "ActorKind")) inst.actorKind = *v;
            if (const std::string* v = Find(section, "ComponentType")) inst.component.componentType = *v;
            if (inst.hasComponent) {
                for (const ComponentFieldDesc& fd : GetComponentFields()) {
                    if (fd.componentType != inst.component.componentType) continue;
                    std::string key = std::string("Comp.") + fd.key;
                    if (fd.type == ComponentFieldType::StringArray) {
                        if (const std::string* cv = Find(section, key + ".Count")) {
                            int count = SafeParse::ToInt(*cv);
                            std::vector<std::string>& arr = inst.component.*(fd.stringArrayField);
                            if (count > 0) arr.resize((size_t)count);
                            for (int a = 0; a < count; a++) {
                                if (const std::string* av = Find(section, key + "." + std::to_string(a)))
                                    arr[(size_t)a] = *av;
                            }
                        }
                        continue;
                    }
                    const std::string* v = Find(section, key);
                    if (!v) continue;
                    if (fd.type == ComponentFieldType::String) inst.component.*(fd.stringField) = *v;
                    else if (fd.type == ComponentFieldType::Float) inst.component.*(fd.floatField) = SafeParse::ToFloat(*v);
                    else if (fd.type == ComponentFieldType::Bool) inst.component.*(fd.boolField) = (*v == "true");
                }
            }
            if (const std::string* v = Find(section, "PosX"))   inst.posX   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "PosY"))   inst.posY   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "PosZ"))   inst.posZ   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "RotX"))   inst.rotX   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "RotY"))   inst.rotY   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "RotZ"))   inst.rotZ   = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "ScaleX")) inst.scaleX = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "ScaleY")) inst.scaleY = SafeParse::ToFloat(*v);
            if (const std::string* v = Find(section, "ScaleZ")) inst.scaleZ = SafeParse::ToFloat(*v);
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