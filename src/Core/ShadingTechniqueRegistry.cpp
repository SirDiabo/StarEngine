#include "ShadingTechniqueRegistry.h"
#include "Module.h"

ShadingTechniqueRegistry& ShadingTechniqueRegistry::Get() {
    static ShadingTechniqueRegistry instance;
    return instance;
}

void ShadingTechniqueRegistry::Register(const ShadingTechnique& technique) {
    for (ShadingTechnique& existing : techniques) {
        if (existing.id == technique.id) {
            existing = technique;
            return;
        }
    }
    techniques.push_back(technique);
}

static bool IsTechniqueAvailable(const ShadingTechnique& technique) {
    return technique.moduleName.empty() || ModuleManager::Get().IsModuleEnabled(technique.moduleName);
}

std::vector<ShadingTechnique> ShadingTechniqueRegistry::GetAvailableTechniques() const {
    std::vector<ShadingTechnique> result;
    for (const ShadingTechnique& technique : techniques) {
        if (IsTechniqueAvailable(technique)) result.push_back(technique);
    }
    return result;
}

std::vector<ShadingTechnique> ShadingTechniqueRegistry::GetAllOptions() const {
    ShadingTechnique unlit;
    unlit.id = "Unlit";
    unlit.displayName = "Unlit";

    std::vector<ShadingTechnique> result;
    result.push_back(unlit);
    for (const ShadingTechnique& technique : GetAvailableTechniques()) result.push_back(technique);
    return result;
}

const ShadingTechnique* ShadingTechniqueRegistry::FindAvailable(const std::string& id) const {
    for (const ShadingTechnique& technique : techniques) {
        if (technique.id != id) continue;
        return IsTechniqueAvailable(technique) ? &technique : nullptr;
    }
    return nullptr;
}
