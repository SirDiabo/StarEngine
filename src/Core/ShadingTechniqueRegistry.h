#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

struct ModuleSettingsResult {
    float nextY = 0.0f;
    bool popupOpen = false;
};

struct ShadingTechnique {
    std::string id;
    std::string displayName;
    std::string moduleName;
    std::function<void(Camera3D, Vector2)> renderPrePass;
    std::function<void(Camera3D, RenderTexture2D)> renderInWorld;
    std::function<void(RenderTexture2D)> renderOverlay;
    std::function<ModuleSettingsResult(float x, float y, float width, int& activeField, bool otherPopupOpen)> renderSettings;
    std::function<std::unordered_map<std::string, std::string>()> collectPackagedSettings;
    std::function<void(const std::unordered_map<std::string, std::string>&)> applyPackagedSettings;
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
