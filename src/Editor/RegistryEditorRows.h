#pragma once
#include "Core/AudioAttenuationRegistry.h"
#include "Core/AudioEffectRegistry.h"
#include "Core/AudioEffectTypes.h"
#include "Core/CollisionLayers.h"
#include "Editor/PropertyRowBuilders.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <vector>

inline std::set<std::string>& ExpandedEffectRows() {
    static std::set<std::string> expanded;
    return expanded;
}

inline const char* NewAttenuationOption() { return "New Attenuation..."; }

inline std::string CreateAttenuationEntry() {
    AudioAttenuationRegistry& registry = AudioAttenuationRegistry::Get();
    std::string name = "Attenuation";
    for (int n = 2; registry.Find(name); n++) name = "Attenuation " + std::to_string(n);
    registry.Add(name);
    return name;
}

inline std::vector<std::string> AttenuationDropdownOptions(const std::string& current) {
    std::vector<std::string> options = { "None" };
    for (const std::string& name : AudioAttenuationRegistry::Get().Names()) options.push_back(name);
    if (!current.empty() && current != "None" && std::find(options.begin(), options.end(), current) == options.end()) options.push_back(current);
    options.push_back(NewAttenuationOption());
    return options;
}

inline std::string ResolveAttenuationChoice(const std::string& choice) {
    return choice == NewAttenuationOption() ? CreateAttenuationEntry() : choice;
}

inline std::string UnitLabel(const std::string& label, const std::string& unit) {
    return unit.empty() ? label : label + " (" + unit + ")";
}

inline void AddAttenuationSettingsRows(std::vector<PropertyRow>& rows, const std::string& idPrefix, const std::string& name,
                                       std::function<void(const std::string&)> onSelect) {
    AudioAttenuationRegistry& registry = AudioAttenuationRegistry::Get();
    const AudioAttenuationDef* def = registry.Find(name);
    if (!def) return;
    const AudioAttenuationSettings settings = AudioAttenuationSchema::MakeSettings(*def);

    std::string section;
    for (const AttenuationParamDef& param : AudioAttenuationSchema::All()) {
        if ((param.id == "curve" || param.id == "min_distance" || param.id == "max_distance") && !settings.distanceEnabled) continue;
        if ((param.id == "spatial_blend" || param.id == "pan_strength") && !settings.spatialization) continue;
        if (param.section == "Occlusion" && param.id != "occlusion" && !settings.occlusion) continue;

        if (param.section != section) {
            section = param.section;
            PropertyRow header = PropertyRows::HeaderRow(idPrefix + ".sec." + section, name + " - " + section);
            rows.push_back(header);
        }

        const std::string rowId = idPrefix + "." + param.id;
        const std::string paramId = param.id;
        PropertyRow row;
        switch (param.kind) {
            case AttenuationParamKind::Bool:
                row = PropertyRows::BoolRow(rowId, param.label, AudioAttenuationSchema::ValueOf(*def, param) >= 0.5f, [name, paramId](bool v) {
                    AudioAttenuationRegistry::Get().SetValue(name, paramId, v ? 1.0f : 0.0f);
                });
                break;
            case AttenuationParamKind::Float:
                row = PropertyRows::SliderRow(rowId, UnitLabel(param.label, param.unit), AudioAttenuationSchema::ValueOf(*def, param), param.minValue,
                    param.id == "min_distance" || param.id == "max_distance" ? 20000.0f : param.maxValue, false,
                    param.id == "max_distance" || param.id == "occlusion_interval" || param.id == "occlusion_smoothing", [name, paramId](float v) {
                        AudioAttenuationRegistry::Get().SetValue(name, paramId, v);
                    });
                break;
            case AttenuationParamKind::Choice: {
                const std::vector<std::string> choices = param.choices;
                const int index = (int)AudioAttenuationSchema::ValueOf(*def, param);
                row = PropertyRows::EnumRow(rowId, param.label, choices[(size_t)std::clamp(index, 0, (int)choices.size() - 1)], choices, [name, paramId, choices](const std::string& v) {
                    auto found = std::find(choices.begin(), choices.end(), v);
                    if (found != choices.end()) AudioAttenuationRegistry::Get().SetValue(name, paramId, (float)(found - choices.begin()));
                });
                row.dropdown = true;
                break;
            }
            case AttenuationParamKind::EffectName: {
                std::vector<std::string> options = { "None" };
                for (const std::string& effect : AudioEffectRegistry::Get().Names()) options.push_back(effect);
                const std::string current = AudioAttenuationSchema::TextOf(*def, param);
                if (!current.empty() && std::find(options.begin(), options.end(), current) == options.end()) options.push_back(current);
                row = PropertyRows::EnumRow(rowId, param.label, current.empty() ? std::string("None") : current, options, [name, paramId](const std::string& v) {
                    AudioAttenuationRegistry::Get().SetText(name, paramId, v == "None" ? std::string() : v);
                });
                row.dropdown = true;
                break;
            }
            case AttenuationParamKind::LayerMask: {
                rows.push_back(PropertyRows::HeaderRow(rowId + ".h", param.label));
                const uint32_t mask = CollisionLayers::ParseMask(AudioAttenuationSchema::TextOf(*def, param));
                for (int layerIndex : CollisionLayers::UsedIndices()) {
                    PropertyRow layerRow = PropertyRows::BoolRow(rowId + "." + std::to_string(layerIndex), CollisionLayers::NameAt(layerIndex),
                        (mask & CollisionLayers::Bit(layerIndex)) != 0, [name, paramId, layerIndex](bool v) {
                            AudioAttenuationRegistry& live = AudioAttenuationRegistry::Get();
                            const AudioAttenuationDef* current = live.Find(name);
                            const AttenuationParamDef* schema = AudioAttenuationSchema::Find(paramId);
                            if (!current || !schema) return;
                            uint32_t bits = CollisionLayers::ParseMask(AudioAttenuationSchema::TextOf(*current, *schema));
                            bits = v ? (bits | CollisionLayers::Bit(layerIndex)) : (bits & ~CollisionLayers::Bit(layerIndex));
                            live.SetText(name, paramId, CollisionLayers::MaskToString(bits));
                        });
                    layerRow.indent = 12.0f;
                    rows.push_back(layerRow);
                }
                continue;
            }
        }
        rows.push_back(row);
    }

    rows.push_back(PropertyRows::ButtonRow(idPrefix + ".delete", "Delete " + name, [name, onSelect]() {
        AudioAttenuationRegistry::Get().Remove(name);
        if (onSelect) onSelect("None");
    }));
}

inline void AddEffectSettingsRows(std::vector<PropertyRow>& rows, const std::string& idPrefix, const std::string& effectName) {
    AudioEffectRegistry& registry = AudioEffectRegistry::Get();
    const AudioEffectDef* def = registry.Find(effectName);
    const AudioEffectTypeInfo* info = def ? AudioEffectTypes::Find(def->type) : nullptr;
    if (!def || !info) return;
    for (const AudioEffectParamDef& param : info->params) {
        const std::string rowId = idPrefix + "." + param.id;
        const std::string paramId = param.id;
        const float value = AudioEffectRegistry::ParamValue(*def, param.id);
        PropertyRow row;
        if (!param.choices.empty()) {
            const std::vector<std::string> choices = param.choices;
            row = PropertyRows::EnumRow(rowId, param.label, choices[(size_t)std::clamp((int)value, 0, (int)choices.size() - 1)], choices,
                [effectName, paramId, choices](const std::string& v) {
                    auto found = std::find(choices.begin(), choices.end(), v);
                    if (found != choices.end()) AudioEffectRegistry::Get().SetParam(effectName, paramId, (float)(found - choices.begin()));
                });
            row.dropdown = true;
        } else if (param.integer && param.minValue == 0.0f && param.maxValue == 1.0f) {
            row = PropertyRows::BoolRow(rowId, param.label, value >= 0.5f, [effectName, paramId](bool v) {
                AudioEffectRegistry::Get().SetParam(effectName, paramId, v ? 1.0f : 0.0f);
            });
        } else {
            row = PropertyRows::SliderRow(rowId, UnitLabel(param.label, param.unit), value, param.minValue, param.maxValue, param.integer, true, [effectName, paramId](float v) {
                AudioEffectRegistry::Get().SetParam(effectName, paramId, v);
            });
        }
        row.indent = 12.0f;
        rows.push_back(row);
    }
}
