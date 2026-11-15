#include "Core/Module.h"

ModuleManager& ModuleManager::Get() {
    static ModuleManager instance;
    return instance;
}

bool ModuleManager::IsModuleEnabled(const std::string&) const {
    return true;
}
