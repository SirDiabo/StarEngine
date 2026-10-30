#pragma once
#include <string>
#include <vector>

class AudioBusRegistry {
public:
    static constexpr int kMaxCustomBuses = 31;

    static const char* Category() { return "AudioBuses"; }
    static const char* MasterName() { return "Master"; }
    static const char* AssetDefaultLabel() { return "Asset Default"; }

    static AudioBusRegistry& Get();

    void EnsureDefaults();

    std::vector<std::string> Names() const;
    std::vector<std::string> CustomNames() const;
    int IndexOf(const std::string& name) const;
    std::string NameAt(int index) const;
    bool Add(const std::string& name);
    bool Remove(const std::string& name);

    std::string EffectiveDefaultBus(const std::string& assetBus) const;
};
