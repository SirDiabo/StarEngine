#include "Module.h"

ModuleManager& ModuleManager::Get() {
    static ModuleManager instance;
    return instance;
}

void ModuleManager::Register(Module* module) {
    modules.push_back(module);
}

bool ModuleManager::InitializeAll() {
    for (auto* module : modules) {
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