#include "ProgrammingInterfaceRegistry.h"

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
