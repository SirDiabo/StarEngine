#include "AudioAttenuationTypes.h"
#include "CollisionLayers.h"
#include <algorithm>
#include <cmath>

namespace {

AttenuationParamDef Flag(const char* id, const char* label, const char* section, bool def) {
    AttenuationParamDef p;
    p.id = id; p.label = label; p.section = section;
    p.kind = AttenuationParamKind::Bool;
    p.minValue = 0.0f; p.maxValue = 1.0f; p.defaultValue = def ? 1.0f : 0.0f;
    return p;
}

AttenuationParamDef Num(const char* id, const char* label, const char* section, float lo, float hi, float def, const char* unit = "") {
    AttenuationParamDef p;
    p.id = id; p.label = label; p.section = section;
    p.kind = AttenuationParamKind::Float;
    p.minValue = lo; p.maxValue = hi; p.defaultValue = def; p.unit = unit;
    return p;
}

AttenuationParamDef Pick(const char* id, const char* label, const char* section, std::vector<std::string> choices, int def) {
    AttenuationParamDef p;
    p.id = id; p.label = label; p.section = section;
    p.kind = AttenuationParamKind::Choice;
    p.minValue = 0.0f; p.maxValue = (float)(choices.size() - 1); p.defaultValue = (float)def;
    p.choices = std::move(choices);
    return p;
}

AttenuationParamDef Mask(const char* id, const char* label, const char* section) {
    AttenuationParamDef p;
    p.id = id; p.label = label; p.section = section;
    p.kind = AttenuationParamKind::LayerMask;
    p.defaultText = CollisionLayers::MaskToString(CollisionLayers::kAllMask);
    return p;
}

AttenuationParamDef Effect(const char* id, const char* label, const char* section) {
    AttenuationParamDef p;
    p.id = id; p.label = label; p.section = section;
    p.kind = AttenuationParamKind::EffectName;
    return p;
}

std::vector<AttenuationParamDef> Build() {
    return {
        Flag("distance_attenuation", "Enable Volume Attenuation", "Volume", true),
        Pick("curve", "Falloff Curve", "Volume", { "Linear", "Inverse", "Logarithmic", "Smooth" }, 0),
        Num("min_distance", "Inner Radius", "Volume", 0.0f, 1000.0f, 2.0f, "m"),
        Num("max_distance", "Falloff End", "Volume", 0.1f, 5000.0f, 30.0f, "m"),
        Flag("spatialization", "Enable Spatialization", "Spatialization", true),
        Num("spatial_blend", "Spatial Blend", "Spatialization", 0.0f, 1.0f, 1.0f),
        Num("pan_strength", "Pan Strength", "Spatialization", 0.0f, 1.0f, 1.0f),
        Flag("occlusion", "Enable Occlusion", "Occlusion", false),
        Mask("occlusion_mask", "Blocked By", "Occlusion"),
        Flag("occlusion_meshes", "Use Meshes", "Occlusion", true),
        Num("occluded_volume", "Occluded Volume", "Occlusion", 0.0f, 1.0f, 0.5f),
        Effect("occluded_effect", "Occluded Effect", "Occlusion"),
        Num("occlusion_smoothing", "Interpolation Time", "Occlusion", 0.01f, 2.0f, 0.15f, "s"),
        Num("occlusion_interval", "Check Interval", "Occlusion", 0.02f, 1.0f, 0.1f, "s"),
    };
}

}

bool AudioAttenuationSettings::SetValue(const std::string& id, float value) {
    const AttenuationParamDef* def = AudioAttenuationSchema::Find(id);
    if (!def || def->kind == AttenuationParamKind::LayerMask || def->kind == AttenuationParamKind::EffectName) return false;
    value = AudioAttenuationSchema::ClampValue(*def, value);
    if (id == "distance_attenuation") distanceEnabled = value >= 0.5f;
    else if (id == "curve") curve = (int)value;
    else if (id == "min_distance") minDistance = value;
    else if (id == "max_distance") maxDistance = value;
    else if (id == "spatialization") spatialization = value >= 0.5f;
    else if (id == "spatial_blend") spatialBlend = value;
    else if (id == "pan_strength") panStrength = value;
    else if (id == "occlusion") occlusion = value >= 0.5f;
    else if (id == "occlusion_meshes") occlusionMeshes = value >= 0.5f;
    else if (id == "occluded_volume") occludedVolume = value;
    else if (id == "occlusion_smoothing") occlusionSmoothing = value;
    else if (id == "occlusion_interval") occlusionInterval = value;
    else return false;
    return true;
}

bool AudioAttenuationSettings::SetText(const std::string& id, const std::string& text) {
    if (id == "occlusion_mask") { occlusionMask = CollisionLayers::ParseMask(text); return true; }
    if (id == "occluded_effect") { occludedEffect = text; return true; }
    return false;
}

namespace AudioAttenuationSchema {

const std::vector<AttenuationParamDef>& All() {
    static const std::vector<AttenuationParamDef> list = Build();
    return list;
}

const AttenuationParamDef* Find(const std::string& id) {
    for (const AttenuationParamDef& p : All()) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

float ClampValue(const AttenuationParamDef& def, float value) {
    if (!std::isfinite(value)) return def.defaultValue;
    value = std::clamp(value, def.minValue, def.maxValue);
    if (def.kind == AttenuationParamKind::Choice) value = std::round(value);
    return value;
}

float ValueOf(const AudioAttenuationDef& def, const AttenuationParamDef& param) {
    auto it = def.values.find(param.id);
    return it == def.values.end() ? param.defaultValue : it->second;
}

std::string TextOf(const AudioAttenuationDef& def, const AttenuationParamDef& param) {
    auto it = def.texts.find(param.id);
    return it == def.texts.end() ? param.defaultText : it->second;
}

AudioAttenuationSettings MakeSettings(const AudioAttenuationDef& def) {
    AudioAttenuationSettings s;
    for (const AttenuationParamDef& p : All()) {
        if (p.kind == AttenuationParamKind::LayerMask || p.kind == AttenuationParamKind::EffectName) s.SetText(p.id, TextOf(def, p));
        else s.SetValue(p.id, ValueOf(def, p));
    }
    if (s.maxDistance <= s.minDistance) s.maxDistance = s.minDistance + 0.01f;
    return s;
}

}
