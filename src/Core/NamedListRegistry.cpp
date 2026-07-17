#include "NamedListRegistry.h"
#include "LayoutSerializer.h"
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

NamedListRegistry& NamedListRegistry::Get() {
    static NamedListRegistry instance;
    return instance;
}

void NamedListRegistry::Load(const std::string& projectRoot) {
    lists.clear();
    filePath = (fs::path(projectRoot) / "Registries.cfg").string();

    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(filePath, raw)) return;

    for (const auto& kv : raw) {
        std::vector<std::string> entries;
        std::stringstream ss(kv.second);
        std::string entry;
        while (std::getline(ss, entry, ',')) {
            if (!entry.empty()) entries.push_back(entry);
        }
        lists[kv.first] = entries;
    }
}

void NamedListRegistry::Save() const {
    if (filePath.empty()) return;

    std::unordered_map<std::string, std::string> raw;
    for (const auto& kv : lists) {
        std::string joined;
        for (size_t i = 0; i < kv.second.size(); i++) {
            if (i > 0) joined += ",";
            joined += kv.second[i];
        }
        raw[kv.first] = joined;
    }
    LayoutSerializer::Save(filePath, raw);
}

const std::vector<std::string>& NamedListRegistry::GetList(const std::string& category) const {
    static const std::vector<std::string> empty;
    auto it = lists.find(category);
    return it != lists.end() ? it->second : empty;
}

bool NamedListRegistry::AddEntry(const std::string& category, const std::string& name) {
    if (name.empty()) return false;
    auto& entries = lists[category];
    if (std::find(entries.begin(), entries.end(), name) != entries.end()) return false;
    entries.push_back(name);
    Save();
    return true;
}

bool NamedListRegistry::HasEntry(const std::string& category, const std::string& name) const {
    const auto& entries = GetList(category);
    return std::find(entries.begin(), entries.end(), name) != entries.end();
}
