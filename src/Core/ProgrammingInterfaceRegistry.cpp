#include "ProgrammingInterfaceRegistry.h"
#include "Module.h"

ProgrammingInterfaceRegistry& ProgrammingInterfaceRegistry::Get() {
    static ProgrammingInterfaceRegistry instance;
    return instance;
}

void ProgrammingInterfaceRegistry::RegisterInterface(const ProgrammingInterfaceInfo& info) {
    for (ProgrammingInterfaceInfo& existing : interfaces) {
        if (existing.name == info.name) {
            existing = info;
            return;
        }
    }
    interfaces.push_back(info);
}

const ProgrammingInterfaceInfo* ProgrammingInterfaceRegistry::Find(const std::string& name) const {
    for (const ProgrammingInterfaceInfo& info : interfaces) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

bool ProgrammingInterfaceInfo::IsAvailable() const {
    return moduleName.empty() || ModuleManager::Get().IsModuleEnabled(moduleName);
}

std::vector<const ProgrammingInterfaceInfo*> ProgrammingInterfaceRegistry::GetAvailableForWidgets() const {
    std::vector<const ProgrammingInterfaceInfo*> result;
    for (const ProgrammingInterfaceInfo& info : interfaces) {
        if (info.SupportsWidgets() && info.IsAvailable()) result.push_back(&info);
    }
    return result;
}

std::vector<const ProgrammingInterfaceInfo*> ProgrammingInterfaceRegistry::GetAvailableForActors() const {
    std::vector<const ProgrammingInterfaceInfo*> result;
    for (const ProgrammingInterfaceInfo& info : interfaces) {
        if (info.SupportsActors() && info.IsAvailable()) result.push_back(&info);
    }
    return result;
}

const ProgrammingInterfaceInfo* ProgrammingInterfaceRegistry::FindByLegacyActorSection(const std::string& sectionName) const {
    if (sectionName.empty()) return nullptr;
    for (const ProgrammingInterfaceInfo& info : interfaces) {
        if (info.legacyActorSection == sectionName) return &info;
    }
    return nullptr;
}
