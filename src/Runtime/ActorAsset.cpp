#include "ActorAsset.h"
#include "ActorBehavior.h"
#include "ComponentRegistry.h"
#include "Core/ContentPathResolver.h"
#include "Core/CookedFile.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Core/SafeParse.h"
#include "raylib.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

using SectionFileIO::ParsedSection;
using SectionFileIO::FindEntry;

ActorAssetData::~ActorAssetData() {
    delete behaviorContent;
}

namespace ActorAsset {

void WriteHeader(std::ostream& out, const ActorAssetHeader& header) {
    out << "[Actor]\n";
    out << "Version=" << header.version << "\n";
    out << "Name=" << header.assetName << "\n";
    out << "ParentClass=" << header.parentClass << "\n";
    out << "ProgrammingInterface=" << header.programmingInterface << "\n";
    out << "\n";
}

void WriteSettings(std::ostream& out, const ActorSettings& settings) {
    const ActorSettings defaults;
    if (settings == defaults) return;
    out << "[ActorSettings]\n";
    if (settings.movementSpeed != defaults.movementSpeed) out << "MovementSpeed=" << settings.movementSpeed << "\n";
    if (settings.acceleration != defaults.acceleration) out << "Acceleration=" << settings.acceleration << "\n";
    if (settings.brakingDeceleration != defaults.brakingDeceleration) out << "BrakingDeceleration=" << settings.brakingDeceleration << "\n";
    if (settings.mass != defaults.mass) out << "Mass=" << settings.mass << "\n";
    if (settings.showMouseCursor != defaults.showMouseCursor) out << "ShowMouseCursor=" << (settings.showMouseCursor ? "true" : "false") << "\n";
    if (settings.useControlYaw != defaults.useControlYaw) out << "UseControlYaw=" << (settings.useControlYaw ? "true" : "false") << "\n";
    if (settings.useControlPitch != defaults.useControlPitch) out << "UseControlPitch=" << (settings.useControlPitch ? "true" : "false") << "\n";
    if (settings.useControlRoll != defaults.useControlRoll) out << "UseControlRoll=" << (settings.useControlRoll ? "true" : "false") << "\n";
    if (settings.tickRate != defaults.tickRate) out << "TickRate=" << settings.tickRate << "\n";
    if (settings.maxTickDistance != defaults.maxTickDistance) out << "MaxTickDistance=" << settings.maxTickDistance << "\n";
    if (settings.farTickRate != defaults.farTickRate) out << "FarTickRate=" << settings.farTickRate << "\n";
    if (settings.maxDrawDistance != defaults.maxDrawDistance) out << "MaxDrawDistance=" << settings.maxDrawDistance << "\n";
    out << "\n";
}

void WriteComponents(std::ostream& out, const std::vector<ComponentDecl>& components) {
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
        const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(comp.componentType);
        if (typeInfo) {
            for (const ComponentFieldDesc& f : typeInfo->fields) {
                auto it = comp.fields.find(f.key);
                if (it == comp.fields.end() || it->second == f.defaultValue) continue;
                out << f.key << "=" << it->second << "\n";
            }
        }
        for (const auto& kv : comp.arrayFields) {
            const auto& arr = kv.second;
            if (!arr.empty()) {
                out << kv.first << "Count=" << arr.size() << "\n";
                for (size_t m = 0; m < arr.size(); m++) out << kv.first << m << "=" << arr[m] << "\n";
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
}

ActorAssetHeader ReadHeader(const std::vector<ParsedSection>& sections) {
    ActorAssetHeader header;
    for (const ParsedSection& section : sections) {
        if (section.name == "Actor") {
            header.found = true;
            if (const std::string* v = FindEntry(section, "Version")) header.version = SafeParse::ToInt(*v);
            if (const std::string* v = FindEntry(section, "Name")) header.assetName = *v;
            if (const std::string* v = FindEntry(section, "ParentClass")) header.parentClass = *v;
            if (const std::string* v = FindEntry(section, "ProgrammingInterface")) header.programmingInterface = *v;
            return header;
        }
    }

    if (!sections.empty()) {
        const ParsedSection& first = sections.front();
        if (const ProgrammingInterfaceInfo* legacy = ProgrammingInterfaceRegistry::Get().FindByLegacyActorSection(first.name)) {
            header.found = true;
            header.isLegacy = true;
            header.legacySection = first.name;
            header.programmingInterface = legacy->name;
            if (const std::string* v = FindEntry(first, "Version")) header.version = SafeParse::ToInt(*v);
            if (const std::string* v = FindEntry(first, "Name")) header.assetName = *v;
            if (const std::string* v = FindEntry(first, "ParentClass")) header.parentClass = *v;
        }
    }
    return header;
}

static float ReadSettingFloat(const ParsedSection& section, const char* key, float fallback) {
    const std::string* v = FindEntry(section, key);
    if (!v) return fallback;
    float value = SafeParse::ToFloat(*v, fallback);
    if (!std::isfinite(value) || value < 0.0f || value > 1.0e9f) return fallback;
    return value;
}

static bool ReadSettingBool(const ParsedSection& section, const char* key, bool fallback) {
    const std::string* v = FindEntry(section, key);
    if (!v) return fallback;
    if (*v == "true" || *v == "1") return true;
    if (*v == "false" || *v == "0") return false;
    return fallback;
}

ActorSettings ReadSettings(const std::vector<ParsedSection>& sections) {
    ActorSettings settings;
    for (const ParsedSection& section : sections) {
        if (section.name != "ActorSettings") continue;
        settings.movementSpeed = ReadSettingFloat(section, "MovementSpeed", settings.movementSpeed);
        settings.acceleration = ReadSettingFloat(section, "Acceleration", settings.acceleration);
        settings.brakingDeceleration = ReadSettingFloat(section, "BrakingDeceleration", settings.brakingDeceleration);
        settings.mass = ReadSettingFloat(section, "Mass", settings.mass);
        settings.showMouseCursor = ReadSettingBool(section, "ShowMouseCursor", settings.showMouseCursor);
        settings.useControlYaw = ReadSettingBool(section, "UseControlYaw", settings.useControlYaw);
        settings.useControlPitch = ReadSettingBool(section, "UseControlPitch", settings.useControlPitch);
        settings.useControlRoll = ReadSettingBool(section, "UseControlRoll", settings.useControlRoll);
        settings.tickRate = ReadSettingFloat(section, "TickRate", settings.tickRate);
        settings.maxTickDistance = ReadSettingFloat(section, "MaxTickDistance", settings.maxTickDistance);
        settings.farTickRate = ReadSettingFloat(section, "FarTickRate", settings.farTickRate);
        settings.maxDrawDistance = ReadSettingFloat(section, "MaxDrawDistance", settings.maxDrawDistance);
        break;
    }
    return settings;
}

void ReadComponents(const std::vector<ParsedSection>& sections, std::vector<ComponentDecl>& outComponents) {
    outComponents.clear();
    for (const ParsedSection& section : sections) {
        if (section.name == "Components" || section.name.rfind("Component", 0) != 0) continue;

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
        std::unordered_set<std::string> consumed = {
            "Type", "Name", "Parent", "PosX", "PosY", "PosZ", "RotX", "RotY", "RotZ",
            "ScaleX", "ScaleY", "ScaleZ", "SavableFields"
        };
        for (const auto& entry : section.entries) {
            const std::string& key = entry.first;
            if (key.size() > 5 && key.compare(key.size() - 5, 5, "Count") == 0) {
                std::string baseKey = key.substr(0, key.size() - 5);
                int count = SafeParse::ToInt(entry.second);
                consumed.insert(key);
                if (count > 0) {
                    std::vector<std::string> arr;
                    arr.resize((size_t)count);
                    for (int m = 0; m < count; m++) {
                        std::string elemKey = baseKey + std::to_string(m);
                        consumed.insert(elemKey);
                        if (const std::string* mv = FindEntry(section, elemKey)) arr[(size_t)m] = *mv;
                    }
                    comp.arrayFields[baseKey] = std::move(arr);
                }
            }
        }
        for (const auto& entry : section.entries) {
            if (consumed.count(entry.first)) continue;
            comp.fields[entry.first] = entry.second;
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
        outComponents.push_back(comp);
    }
}

bool Write(std::ostream& out, const ActorAssetData& data) {
    ActorAssetHeader header;
    header.assetName = data.assetName;
    header.parentClass = data.parentClass;
    header.programmingInterface = data.programmingInterface;
    WriteHeader(out, header);
    WriteSettings(out, data.settings);
    WriteComponents(out, data.components);

    if (!data.programmingInterface.empty() && data.behaviorContent) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(data.programmingInterface);
        if (provider && provider->saveActorContent) provider->saveActorContent(out, *data.behaviorContent, ContentPrefix);
    }
    return true;
}

bool Save(const std::string& path, const ActorAssetData& data) {
    std::string resolvedPath = ContentPathResolver::Resolve(path);
    std::ofstream out(resolvedPath);
    if (!out.is_open()) {
        TraceLog(LOG_WARNING, "ActorAsset: failed to open \"%s\" for writing", resolvedPath.c_str());
        return false;
    }
    return Write(out, data);
}

bool Read(std::istream& in, ActorAssetData& outData, bool loadBehavior) {
    std::vector<ParsedSection> sections = SectionFileIO::ParseSections(in);
    ActorAssetHeader header = ReadHeader(sections);
    if (!header.found) return false;

    outData.assetName = header.assetName;
    outData.parentClass = header.parentClass;
    outData.programmingInterface = header.programmingInterface;
    outData.settings = ReadSettings(sections);
    ReadComponents(sections, outData.components);

    delete outData.behaviorContent;
    outData.behaviorContent = nullptr;
    if (loadBehavior && !header.programmingInterface.empty()) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(header.programmingInterface);
        if (provider && provider->loadActorContent) {
            outData.behaviorContent = provider->loadActorContent(sections, header.isLegacy ? std::string() : std::string(ContentPrefix));
        }
    }
    return true;
}

bool Load(const std::string& path, ActorAssetData& outData, bool loadBehavior) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return false;
    return Read(in, outData, loadBehavior);
}

std::string PeekInterface(const std::string& path) {
    std::ifstream in(CookedFile::Open(path));
    if (!in.is_open()) return std::string();
    ActorAssetHeader header = ReadHeader(SectionFileIO::ParseSections(in));
    return header.found ? header.programmingInterface : std::string();
}

ActorBehavior* CreateBehavior(const ActorAssetData& data) {
    if (data.programmingInterface.empty() || !data.behaviorContent) return nullptr;
    const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(data.programmingInterface);
    if (!provider || !provider->createActorBehavior) return nullptr;
    return provider->createActorBehavior(*data.behaviorContent, data.components);
}

}
