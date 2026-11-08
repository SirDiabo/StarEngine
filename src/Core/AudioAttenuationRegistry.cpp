#include "AudioAttenuationRegistry.h"
#include "AudioEffectRegistry.h"
#include "SafeParse.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string FormatFloat(float value) {
    char buffer[48];
    for (int precision = 6; precision <= 9; precision++) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, (double)value);
        if (SafeParse::ToFloat(buffer, value + 1.0f) == value) break;
    }
    return buffer;
}

}

AudioAttenuationRegistry& AudioAttenuationRegistry::Get() {
    static AudioAttenuationRegistry instance;
    return instance;
}

void AudioAttenuationRegistry::AddListener(std::function<void()> listener) {
    if (listener) listeners.push_back(std::move(listener));
}

void AudioAttenuationRegistry::AddParamListener(std::function<void(const std::string&)> listener) {
    if (listener) paramListeners.push_back(std::move(listener));
}

void AudioAttenuationRegistry::Changed(bool persist) {
    revision++;
    if (persist) Save();
    for (const auto& listener : listeners) listener();
}

void AudioAttenuationRegistry::Clear() {
    entries.clear();
    filePath.clear();
    Changed(false);
}

void AudioAttenuationRegistry::Load(const std::string& projectRoot) {
    std::string path = (fs::path(projectRoot) / "Config" / "AudioAttenuations.cfg").string();
    std::string text;
    std::ifstream in(path, std::ios::binary);
    if (in.is_open()) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        text = buffer.str();
    }
    LoadFromText(text);
    filePath = path;
}

void AudioAttenuationRegistry::LoadFromText(const std::string& text) {
    entries.clear();
    filePath.clear();

    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("attenuation.", 0) != 0) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(12, eq - 12);
        std::string value = line.substr(eq + 1);
        size_t dot = key.find('.');
        if (dot == std::string::npos) continue;
        std::string name = key.substr(0, dot);
        std::string field = key.substr(dot + 1);
        if (!AudioEffectRegistry::IsValidName(name)) continue;

        AudioAttenuationDef* def = FindMutable(name);
        if (!def) {
            entries.push_back({});
            entries.back().name = name;
            def = &entries.back();
        }
        const AttenuationParamDef* param = AudioAttenuationSchema::Find(field);
        if (!param) continue;
        if (param->kind == AttenuationParamKind::LayerMask || param->kind == AttenuationParamKind::EffectName) def->texts[field] = value;
        else def->values[field] = AudioAttenuationSchema::ClampValue(*param, SafeParse::ToFloat(value, param->defaultValue));
    }
    Changed(false);
}

std::string AudioAttenuationRegistry::ToText() const {
    std::string out;
    for (const AudioAttenuationDef& def : entries) {
        const std::string prefix = "attenuation." + def.name + ".";
        for (const AttenuationParamDef& p : AudioAttenuationSchema::All()) {
            if (p.kind == AttenuationParamKind::LayerMask || p.kind == AttenuationParamKind::EffectName) out += prefix + p.id + "=" + AudioAttenuationSchema::TextOf(def, p) + "\n";
            else out += prefix + p.id + "=" + FormatFloat(AudioAttenuationSchema::ValueOf(def, p)) + "\n";
        }
    }
    return out;
}

bool AudioAttenuationRegistry::Save() const {
    if (filePath.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(filePath).parent_path(), ec);
    std::string tempPath = filePath + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary);
        if (!out.is_open()) return false;
        out << ToText();
        if (!out.good()) return false;
    }
    fs::rename(tempPath, filePath, ec);
    if (ec) {
        fs::remove(filePath, ec);
        fs::rename(tempPath, filePath, ec);
    }
    return !ec;
}

std::vector<std::string> AudioAttenuationRegistry::Names() const {
    std::vector<std::string> names;
    for (const AudioAttenuationDef& def : entries) names.push_back(def.name);
    return names;
}

AudioAttenuationDef* AudioAttenuationRegistry::FindMutable(const std::string& name) {
    for (AudioAttenuationDef& def : entries) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

const AudioAttenuationDef* AudioAttenuationRegistry::Find(const std::string& name) const {
    for (const AudioAttenuationDef& def : entries) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

bool AudioAttenuationRegistry::Add(const std::string& name) {
    if (!AudioEffectRegistry::IsValidName(name) || Find(name)) return false;
    AudioAttenuationDef def;
    def.name = name;
    for (const AttenuationParamDef& p : AudioAttenuationSchema::All()) {
        if (p.kind == AttenuationParamKind::LayerMask || p.kind == AttenuationParamKind::EffectName) def.texts[p.id] = p.defaultText;
        else def.values[p.id] = p.defaultValue;
    }
    entries.push_back(std::move(def));
    Changed(true);
    return true;
}

bool AudioAttenuationRegistry::Remove(const std::string& name) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](const AudioAttenuationDef& d) { return d.name == name; });
    if (it == entries.end()) return false;
    entries.erase(it);
    Changed(true);
    return true;
}

bool AudioAttenuationRegistry::Rename(const std::string& oldName, const std::string& newName) {
    if (oldName == newName) return true;
    if (!AudioEffectRegistry::IsValidName(newName) || Find(newName)) return false;
    AudioAttenuationDef* def = FindMutable(oldName);
    if (!def) return false;
    def->name = newName;
    Changed(true);
    return true;
}

bool AudioAttenuationRegistry::SetValue(const std::string& name, const std::string& paramId, float value, bool persist) {
    AudioAttenuationDef* def = FindMutable(name);
    const AttenuationParamDef* param = AudioAttenuationSchema::Find(paramId);
    if (!def || !param || param->kind == AttenuationParamKind::LayerMask || param->kind == AttenuationParamKind::EffectName) return false;
    def->values[paramId] = AudioAttenuationSchema::ClampValue(*param, value);
    revision++;
    if (persist) Save();
    for (const auto& listener : paramListeners) listener(name);
    return true;
}

bool AudioAttenuationRegistry::SetText(const std::string& name, const std::string& paramId, const std::string& text) {
    AudioAttenuationDef* def = FindMutable(name);
    const AttenuationParamDef* param = AudioAttenuationSchema::Find(paramId);
    if (!def || !param || (param->kind != AttenuationParamKind::LayerMask && param->kind != AttenuationParamKind::EffectName)) return false;
    def->texts[paramId] = text;
    revision++;
    Save();
    for (const auto& listener : paramListeners) listener(name);
    return true;
}
