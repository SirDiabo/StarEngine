#include "Module.h"
#include "Project.h"
#include "EngineRoot.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

ModuleManager& ModuleManager::Get() {
    static ModuleManager instance;
    return instance;
}

void ModuleManager::Register(Module* module) {
    modules.push_back(module);
    RebuildEntries();
}

bool ModuleManager::InitializeAll() {
    for (auto* module : modules) {
        if (!module->IsEnabled()) continue;
        if (!module->Initialize()) return false;
    }
    return true;
}

void ModuleManager::ShutdownAll() {
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
        (*it)->Shutdown();
    }
}

void ModuleManager::UpdateAll(float deltaTime) {
    for (auto* module : modules) {
        if (module->IsEnabled()) module->Update(deltaTime);
    }
}

std::string ModuleManager::GetModulesSourceRoot() {
    return (fs::path(EngineRoot::Resolve()) / "src" / "Modules").string();
}

std::string ModuleManager::GetEngineModulesRoot() {
    return (fs::path(EngineRoot::Resolve()) / "Modules").string();
}

std::string ModuleManager::GetProjectModulesRoot() {
    const std::string& rootPath = Project::Get().GetRootPath();
    if (rootPath.empty()) return "";
    return Project::Get().GetModulesDirectory();
}

static void ScanManifestsUnder(const std::string& modulesRoot, std::vector<ModuleEntry>& entries) {
    if (modulesRoot.empty() || !fs::exists(modulesRoot) || !fs::is_directory(modulesRoot)) return;

    for (const auto& dirEntry : fs::directory_iterator(modulesRoot)) {
        if (!dirEntry.is_directory()) continue;
        for (const auto& fileEntry : fs::directory_iterator(dirEntry.path())) {
            if (fileEntry.path().extension() != ".starmodule") continue;

            ModuleEntry entry;
            if (ModuleManifest::LoadFromFile(fileEntry.path().string(), entry.manifest)) {
                entry.hasManifest = true;
                entries.push_back(entry);
            }
            break;
        }
    }
}

void ModuleManager::ScanManifests() {
    entries.clear();

    ScanManifestsUnder(GetModulesSourceRoot(), entries);
    ScanManifestsUnder(GetProjectModulesRoot(), entries);

    std::vector<ModuleEntry> engineEntries;
    ScanManifestsUnder(GetEngineModulesRoot(), engineEntries);
    for (ModuleEntry& engineEntry : engineEntries) {
        bool alreadyClaimedByProject = false;
        for (const ModuleEntry& existing : entries) {
            if (existing.manifest.name == engineEntry.manifest.name) {
                alreadyClaimedByProject = true;
                break;
            }
        }
        if (!alreadyClaimedByProject) entries.push_back(engineEntry);
    }

    for (Module* module : modules) {
        bool matched = false;
        for (ModuleEntry& entry : entries) {
            if (entry.manifest.name == module->GetName()) {
                entry.instance = module;
                matched = true;
                break;
            }
        }
        if (!matched) {
            ModuleEntry entry;
            entry.instance = module;
            entry.hasManifest = false;
            entries.push_back(entry);
        }
    }
}

void ModuleManager::RebuildEntries() {
    // Keep manifest associations in sync whenever a module registers itself.
    ScanManifests();
}

ModuleEntry* ModuleManager::FindEntry(const std::string& moduleName) {
    for (ModuleEntry& entry : entries) {
        std::string entryName = entry.instance ? entry.instance->GetName() : entry.manifest.name;
        if (entryName == moduleName) return &entry;
    }
    return nullptr;
}

void ModuleManager::SetModuleEnabled(const std::string& moduleName, bool value) {
    ModuleEntry* entry = FindEntry(moduleName);
    if (entry && entry->instance) entry->instance->SetEnabled(value);
}

bool ModuleManager::IsModuleEnabled(const std::string& moduleName) const {
    for (const ModuleEntry& entry : entries) {
        std::string entryName = entry.instance ? entry.instance->GetName() : entry.manifest.name;
        if (entryName == moduleName) return entry.instance ? entry.instance->IsEnabled() : false;
    }
    return false;
}

std::vector<std::string> ModuleManager::GetDisabledModuleNames() const {
    std::vector<std::string> result;
    for (const ModuleEntry& entry : entries) {
        if (entry.instance && !entry.instance->IsEnabled()) {
            result.push_back(entry.instance->GetName());
        }
    }
    return result;
}

void ModuleManager::ApplyDisabledModuleNames(const std::vector<std::string>& disabledNames) {
    for (Module* module : modules) {
        bool shouldDisable = std::find(disabledNames.begin(), disabledNames.end(), module->GetName()) != disabledNames.end();
        module->SetEnabled(!shouldDisable);
    }
}