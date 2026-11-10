#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class AttenuationParamKind { Bool, Float, Choice, LayerMask, EffectName };

struct AttenuationParamDef {
    std::string id;
    std::string label;
    std::string section;
    AttenuationParamKind kind = AttenuationParamKind::Float;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::vector<std::string> choices;
    std::string defaultText;
    std::string unit;
};

struct AudioAttenuationDef {
    std::string name;
    std::map<std::string, float> values;
    std::map<std::string, std::string> texts;
};

struct AudioAttenuationSettings {
    bool distanceEnabled = true;
    int curve = 0;
    float minDistance = 200.0f;
    float maxDistance = 5000.0f;
    bool spatialization = true;
    float spatialBlend = 1.0f;
    float panStrength = 1.0f;
    bool occlusion = false;
    uint32_t occlusionMask = 0xFFFFFFFFu;
    bool occlusionMeshes = true;
    float occludedVolume = 0.5f;
    std::string occludedEffect;
    float occlusionSmoothing = 0.15f;
    float occlusionInterval = 0.1f;

    bool RequiresPosition() const { return distanceEnabled || spatialization || occlusion; }
    bool SetValue(const std::string& id, float value);
    bool SetText(const std::string& id, const std::string& text);
};

namespace AudioAttenuationSchema {

const std::vector<AttenuationParamDef>& All();
const AttenuationParamDef* Find(const std::string& id);
float ClampValue(const AttenuationParamDef& def, float value);
AudioAttenuationSettings MakeSettings(const AudioAttenuationDef& def);
float ValueOf(const AudioAttenuationDef& def, const AttenuationParamDef& param);
std::string TextOf(const AudioAttenuationDef& def, const AttenuationParamDef& param);

}
