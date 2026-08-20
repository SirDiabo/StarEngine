#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <functional>

struct ShadingTechnique {
    std::string id;
    std::string displayName;
    std::string moduleName;
    std::function<void(Camera3D, Vector2)> renderPrePass;
    std::function<void(Camera3D, RenderTexture2D)> renderInWorld;
    std::function<void(RenderTexture2D)> renderOverlay;
};

class ShadingTechniqueRegistry {
public:
    static ShadingTechniqueRegistry& Get();

    void Register(const ShadingTechnique& technique);

    std::vector<ShadingTechnique> GetAvailableTechniques() const;
    std::vector<ShadingTechnique> GetAllOptions() const;
    const ShadingTechnique* FindAvailable(const std::string& id) const;

private:
    std::vector<ShadingTechnique> techniques;
};
