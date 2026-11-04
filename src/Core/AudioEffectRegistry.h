#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct AudioEffectDef {
    std::string name;
    std::string type;
    std::map<std::string, float> params;
};

class AudioEffectRegistry {
public:
    static AudioEffectRegistry& Get();

    static bool IsValidName(const std::string& name);

    void Load(const std::string& projectRoot);
    void LoadFromText(const std::string& text);
    bool Save() const;
    std::string ToText() const;
    void Clear();

    std::vector<std::string> Names() const;
    const AudioEffectDef* Find(const std::string& name) const;
    bool Add(const std::string& name, const std::string& type);
    bool Remove(const std::string& name);
    bool Rename(const std::string& oldName, const std::string& newName);
    bool SetParam(const std::string& name, const std::string& paramId, float value);
    float GetParam(const std::string& name, const std::string& paramId) const;
    static float ParamValue(const AudioEffectDef& def, const std::string& paramId);

    std::vector<std::string> BusEffects(const std::string& busName) const;
    void SetBusEffects(const std::string& busName, const std::vector<std::string>& effectNames);

    uint64_t Revision() const { return revision; }
    void AddListener(std::function<void()> listener);

private:
    AudioEffectDef* FindMutable(const std::string& name);
    void Changed(bool persist);

    std::string filePath;
    std::vector<AudioEffectDef> effects;
    std::vector<std::pair<std::string, std::vector<std::string>>> busLists;
    std::vector<std::function<void()>> listeners;
    uint64_t revision = 0;
};
