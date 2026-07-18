#include "SaveFileRegistry.h"
#include "LayoutSerializer.h"
#include <filesystem>

namespace fs = std::filesystem;

SaveFileRegistry& SaveFileRegistry::Get() {
    static SaveFileRegistry instance;
    return instance;
}

void SaveFileRegistry::Load(const std::string& projectRoot) {
    encryptedFlags.clear();
    filePath = (fs::path(projectRoot) / "SaveFileSettings.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    for (const auto& kv : raw) {
        encryptedFlags[kv.first] = (kv.second == "1");
    }
}

void SaveFileRegistry::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    for (const auto& kv : encryptedFlags) {
        raw[kv.first] = kv.second ? "1" : "0";
    }
    LayoutSerializer::Save(filePath, raw);
}

bool SaveFileRegistry::IsEncrypted(const std::string& name) const {
    auto it = encryptedFlags.find(name);
    return it != encryptedFlags.end() && it->second;
}

void SaveFileRegistry::SetEncrypted(const std::string& name, bool encrypted) {
    encryptedFlags[name] = encrypted;
    Save();
}
