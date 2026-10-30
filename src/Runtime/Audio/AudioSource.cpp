#include "AudioSource.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

AudioSourceRegistry& AudioSourceRegistry::Get() {
    static AudioSourceRegistry instance;
    return instance;
}

void AudioSourceRegistry::Register(const AudioSourceType& type) {
    for (AudioSourceType& existing : types) {
        if (existing.extension == type.extension) {
            existing = type;
            return;
        }
    }
    types.push_back(type);
}

const AudioSourceType* AudioSourceRegistry::FindForPath(const std::string& assetPath) const {
    std::string ext = std::filesystem::path(assetPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    for (const AudioSourceType& type : types) {
        if (type.extension == ext) return &type;
    }
    return nullptr;
}

std::unique_ptr<AudioSource> AudioSourceRegistry::Create(const std::string& assetPath) const {
    const AudioSourceType* type = FindForPath(assetPath);
    if (!type || !type->create) return nullptr;
    return type->create(assetPath);
}

bool AudioSourceRegistry::DescribeParameters(const std::string& assetPath, std::vector<AudioParamInfo>& out) const {
    const AudioSourceType* type = FindForPath(assetPath);
    if (!type || !type->describeParameters) return false;
    type->describeParameters(assetPath, out);
    return true;
}

std::vector<std::string> AudioSourceRegistry::GetExtensions() const {
    std::vector<std::string> result;
    for (const AudioSourceType& type : types) result.push_back(type.extension);
    return result;
}
