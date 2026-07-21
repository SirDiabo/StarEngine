#include "InputMappingRegistry.h"
#include "LayoutSerializer.h"
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string TypeToString(InputMappingType type) {
    switch (type) {
        case InputMappingType::Axis:   return "Axis";
        case InputMappingType::Axis2D: return "Axis2D";
        default:                       return "Bool";
    }
}

InputMappingType TypeFromString(const std::string& s) {
    if (s == "Axis") return InputMappingType::Axis;
    if (s == "Axis2D") return InputMappingType::Axis2D;
    return InputMappingType::Bool;
}

std::string EncodeMapping(const InputMapping& mapping) {
    std::string out = TypeToString(mapping.type) + "|";
    for (size_t i = 0; i < mapping.bindings.size(); i++) {
        if (i > 0) out += ",";
        const InputMappingBinding& b = mapping.bindings[i];
        out += b.key + ":" + std::to_string(b.scale) + ":" + std::to_string(b.axis);
    }
    return out;
}

InputMapping DecodeMapping(const std::string& raw) {
    InputMapping mapping;
    size_t bar = raw.find('|');
    if (bar == std::string::npos) return mapping;

    mapping.type = TypeFromString(raw.substr(0, bar));

    std::stringstream ss(raw.substr(bar + 1));
    std::string entry;
    while (std::getline(ss, entry, ',')) {
        if (entry.empty()) continue;
        size_t c1 = entry.find(':');
        if (c1 == std::string::npos) continue;
        size_t c2 = entry.find(':', c1 + 1);
        if (c2 == std::string::npos) continue;

        InputMappingBinding binding;
        binding.key   = entry.substr(0, c1);
        binding.scale = std::stof(entry.substr(c1 + 1, c2 - c1 - 1));
        binding.axis  = std::stoi(entry.substr(c2 + 1));
        mapping.bindings.push_back(binding);
    }
    return mapping;
}

}

InputMappingRegistry& InputMappingRegistry::Get() {
    static InputMappingRegistry instance;
    return instance;
}

void InputMappingRegistry::Load(const std::string& projectRoot) {
    mappings.clear();
    filePath = (fs::path(projectRoot) / "Saved" / "InputMappings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    for (const auto& kv : raw) {
        mappings[kv.first] = DecodeMapping(kv.second);
    }
}

void InputMappingRegistry::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    for (const auto& kv : mappings) {
        raw[kv.first] = EncodeMapping(kv.second);
    }
    LayoutSerializer::Save(filePath, raw);
}

const InputMapping* InputMappingRegistry::Find(const std::string& name) const {
    auto it = mappings.find(name);
    return it != mappings.end() ? &it->second : nullptr;
}

void InputMappingRegistry::SetMapping(const std::string& name, const InputMapping& mapping) {
    mappings[name] = mapping;
    Save();
}

void InputMappingRegistry::Remove(const std::string& name) {
    mappings.erase(name);
    Save();
}
