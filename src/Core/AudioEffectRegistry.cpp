#include "AudioEffectRegistry.h"
#include "AudioEffectTypes.h"
#include "SafeParse.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::vector<std::string> SplitList(const std::string& text) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string FormatFloat(float value) {
    char buffer[48];
    for (int precision = 6; precision <= 9; precision++) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, (double)value);
        if (SafeParse::ToFloat(buffer, value + 1.0f) == value) break;
    }
    return buffer;
}

}

AudioEffectRegistry& AudioEffectRegistry::Get() {
    static AudioEffectRegistry instance;
    return instance;
}

bool AudioEffectRegistry::IsValidName(const std::string& name) {
    if (name.empty() || name.size() > 48) return false;
    if (name.front() == ' ' || name.back() == ' ') return false;
    for (unsigned char c : name) {
        if (c < 32 || c == '.' || c == '=' || c == ';' || c == ',') return false;
    }
    return true;
}

void AudioEffectRegistry::AddListener(std::function<void()> listener) {
    if (listener) listeners.push_back(std::move(listener));
}

void AudioEffectRegistry::AddParamListener(std::function<void(const std::string&, const std::string&, float)> listener) {
    if (listener) paramListeners.push_back(std::move(listener));
}

void AudioEffectRegistry::Changed(bool persist) {
    revision++;
    if (persist) Save();
    for (const auto& listener : listeners) listener();
}

void AudioEffectRegistry::Clear() {
    effects.clear();
    busLists.clear();
    filePath.clear();
    Changed(false);
}

void AudioEffectRegistry::Load(const std::string& projectRoot) {
    std::string path = (fs::path(projectRoot) / "Config" / "AudioEffects.cfg").string();
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

void AudioEffectRegistry::LoadFromText(const std::string& text) {
    effects.clear();
    busLists.clear();
    filePath.clear();

    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key.rfind("bus.", 0) == 0) {
            std::string busName = key.substr(4);
            if (!busName.empty()) busLists.push_back({ busName, SplitList(value) });
            continue;
        }
        if (key.rfind("effect.", 0) != 0) continue;

        std::string rest = key.substr(7);
        size_t dot = rest.find('.');
        if (dot == std::string::npos) continue;
        std::string name = rest.substr(0, dot);
        std::string field = rest.substr(dot + 1);
        if (!IsValidName(name)) continue;

        AudioEffectDef* def = FindMutable(name);
        if (!def) {
            effects.push_back({ name, std::string(), {} });
            def = &effects.back();
        }
        if (field == "type") {
            def->type = value;
        } else if (field.rfind("param.", 0) == 0) {
            std::string paramId = field.substr(6);
            if (!paramId.empty()) def->params[paramId] = SafeParse::ToFloat(value, 0.0f);
        }
    }

    for (AudioEffectDef& def : effects) {
        const AudioEffectTypeInfo* info = AudioEffectTypes::Find(def.type);
        if (!info) continue;
        for (auto it = def.params.begin(); it != def.params.end();) {
            int index = AudioEffectTypes::ParamIndex(*info, it->first);
            if (index < 0) { it = def.params.erase(it); continue; }
            it->second = AudioEffectTypes::ClampParam(info->params[(size_t)index], it->second);
            ++it;
        }
    }

    Changed(false);
}

std::string AudioEffectRegistry::ToText() const {
    std::string out;
    for (const AudioEffectDef& def : effects) {
        out += "effect." + def.name + ".type=" + def.type + "\n";
        const AudioEffectTypeInfo* info = AudioEffectTypes::Find(def.type);
        if (!info) continue;
        for (const AudioEffectParamDef& param : info->params) {
            out += "effect." + def.name + ".param." + param.id + "=" + FormatFloat(ParamValue(def, param.id)) + "\n";
        }
    }
    for (const auto& entry : busLists) {
        if (entry.second.empty()) continue;
        std::string joined;
        for (size_t i = 0; i < entry.second.size(); i++) {
            if (i > 0) joined += ";";
            joined += entry.second[i];
        }
        out += "bus." + entry.first + "=" + joined + "\n";
    }
    return out;
}

bool AudioEffectRegistry::Save() const {
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

std::vector<std::string> AudioEffectRegistry::Names() const {
    std::vector<std::string> names;
    for (const AudioEffectDef& def : effects) names.push_back(def.name);
    return names;
}

AudioEffectDef* AudioEffectRegistry::FindMutable(const std::string& name) {
    for (AudioEffectDef& def : effects) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

const AudioEffectDef* AudioEffectRegistry::Find(const std::string& name) const {
    for (const AudioEffectDef& def : effects) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

bool AudioEffectRegistry::Add(const std::string& name, const std::string& type) {
    if (!IsValidName(name) || Find(name)) return false;
    const AudioEffectTypeInfo* info = AudioEffectTypes::Find(type);
    if (!info) return false;
    AudioEffectDef def;
    def.name = name;
    def.type = type;
    for (const AudioEffectParamDef& param : info->params) def.params[param.id] = param.defaultValue;
    effects.push_back(std::move(def));
    Changed(true);
    return true;
}

bool AudioEffectRegistry::Remove(const std::string& name) {
    auto it = std::find_if(effects.begin(), effects.end(), [&](const AudioEffectDef& d) { return d.name == name; });
    if (it == effects.end()) return false;
    effects.erase(it);
    for (auto& entry : busLists) {
        entry.second.erase(std::remove(entry.second.begin(), entry.second.end(), name), entry.second.end());
    }
    busLists.erase(std::remove_if(busLists.begin(), busLists.end(), [](const auto& e) { return e.second.empty(); }), busLists.end());
    Changed(true);
    return true;
}

bool AudioEffectRegistry::Rename(const std::string& oldName, const std::string& newName) {
    if (oldName == newName) return true;
    if (!IsValidName(newName) || Find(newName)) return false;
    AudioEffectDef* def = FindMutable(oldName);
    if (!def) return false;
    def->name = newName;
    for (auto& entry : busLists) {
        for (std::string& item : entry.second) {
            if (item == oldName) item = newName;
        }
    }
    Changed(true);
    return true;
}

float AudioEffectRegistry::ParamValue(const AudioEffectDef& def, const std::string& paramId) {
    auto it = def.params.find(paramId);
    if (it != def.params.end()) return it->second;
    const AudioEffectTypeInfo* info = AudioEffectTypes::Find(def.type);
    if (!info) return 0.0f;
    int index = AudioEffectTypes::ParamIndex(*info, paramId);
    return index < 0 ? 0.0f : info->params[(size_t)index].defaultValue;
}

bool AudioEffectRegistry::SetParam(const std::string& name, const std::string& paramId, float value, bool persist) {
    AudioEffectDef* def = FindMutable(name);
    if (!def) return false;
    const AudioEffectTypeInfo* info = AudioEffectTypes::Find(def->type);
    if (!info) return false;
    int index = AudioEffectTypes::ParamIndex(*info, paramId);
    if (index < 0) return false;
    const float clamped = AudioEffectTypes::ClampParam(info->params[(size_t)index], value);
    def->params[paramId] = clamped;
    revision++;
    if (persist) Save();
    for (const auto& listener : paramListeners) listener(name, paramId, clamped);
    return true;
}

float AudioEffectRegistry::GetParam(const std::string& name, const std::string& paramId) const {
    const AudioEffectDef* def = Find(name);
    return def ? ParamValue(*def, paramId) : 0.0f;
}

std::vector<std::string> AudioEffectRegistry::BusEffects(const std::string& busName) const {
    for (const auto& entry : busLists) {
        if (entry.first == busName) return entry.second;
    }
    return {};
}

void AudioEffectRegistry::SetBusEffects(const std::string& busName, const std::vector<std::string>& effectNames) {
    if (busName.empty()) return;
    bool found = false;
    for (auto& entry : busLists) {
        if (entry.first == busName) { entry.second = effectNames; found = true; break; }
    }
    if (!found) busLists.push_back({ busName, effectNames });
    busLists.erase(std::remove_if(busLists.begin(), busLists.end(), [](const auto& e) { return e.second.empty(); }), busLists.end());
    Changed(true);
}
