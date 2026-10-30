#include "AudioBusRegistry.h"
#include "NamedListRegistry.h"

AudioBusRegistry& AudioBusRegistry::Get() {
    static AudioBusRegistry instance;
    return instance;
}

void AudioBusRegistry::EnsureDefaults() {
    NamedListRegistry& lists = NamedListRegistry::Get();
    if (lists.HasCategory(Category())) return;
    lists.SetList(Category(), { "SFX", "Music" });
}

std::vector<std::string> AudioBusRegistry::CustomNames() const {
    return NamedListRegistry::Get().GetList(Category());
}

std::vector<std::string> AudioBusRegistry::Names() const {
    std::vector<std::string> names;
    names.push_back(MasterName());
    for (const std::string& name : CustomNames()) names.push_back(name);
    return names;
}

int AudioBusRegistry::IndexOf(const std::string& name) const {
    if (name == MasterName()) return 0;
    const std::vector<std::string>& custom = NamedListRegistry::Get().GetList(Category());
    for (size_t i = 0; i < custom.size() && i < (size_t)kMaxCustomBuses; i++) {
        if (custom[i] == name) return (int)i + 1;
    }
    return -1;
}

std::string AudioBusRegistry::NameAt(int index) const {
    if (index == 0) return MasterName();
    const std::vector<std::string>& custom = NamedListRegistry::Get().GetList(Category());
    if (index > 0 && (size_t)index <= custom.size()) return custom[(size_t)index - 1];
    return std::string();
}

bool AudioBusRegistry::Add(const std::string& name) {
    if (name.empty() || name == MasterName() || name == AssetDefaultLabel()) return false;
    if ((int)CustomNames().size() >= kMaxCustomBuses) return false;
    return NamedListRegistry::Get().AddEntry(Category(), name);
}

bool AudioBusRegistry::Remove(const std::string& name) {
    return NamedListRegistry::Get().RemoveEntry(Category(), name);
}

std::string AudioBusRegistry::EffectiveDefaultBus(const std::string& assetBus) const {
    if (!assetBus.empty() && IndexOf(assetBus) >= 0) return assetBus;
    if (assetBus.empty() && IndexOf("SFX") >= 0) return "SFX";
    return MasterName();
}
