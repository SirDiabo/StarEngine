#pragma once
#include "AudioAttenuationTypes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class AudioAttenuationRegistry {
public:
    static AudioAttenuationRegistry& Get();

    void Load(const std::string& projectRoot);
    void LoadFromText(const std::string& text);
    bool Save() const;
    std::string ToText() const;
    void Clear();

    std::vector<std::string> Names() const;
    const AudioAttenuationDef* Find(const std::string& name) const;
    bool Add(const std::string& name);
    bool Remove(const std::string& name);
    bool Rename(const std::string& oldName, const std::string& newName);
    bool SetValue(const std::string& name, const std::string& paramId, float value, bool persist = true);
    bool SetText(const std::string& name, const std::string& paramId, const std::string& text);

    uint64_t Revision() const { return revision; }
    void AddListener(std::function<void()> listener);
    void AddParamListener(std::function<void(const std::string&)> listener);

private:
    AudioAttenuationDef* FindMutable(const std::string& name);
    void Changed(bool persist);

    std::string filePath;
    std::vector<AudioAttenuationDef> entries;
    std::vector<std::function<void()>> listeners;
    std::vector<std::function<void(const std::string&)>> paramListeners;
    uint64_t revision = 0;
};
