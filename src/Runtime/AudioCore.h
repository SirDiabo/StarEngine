#pragma once
#include "Audio/AudioSource.h"
#include "Audio/WavFile.h"
#include <memory>
#include <string>
#include <unordered_map>

class AudioCore {
public:
    static AudioCore& Get();

    void InitDevice();
    void CloseDevice();

    void Tick(float deltaTime);

    std::shared_ptr<const SampleData> GetSampleData(const std::string& resolvedWavPath);
    void ClearSampleCache();

    int PlayAsset(const std::string& assetPath, float volume = 1.0f, float pitch = 1.0f, bool looping = false);
    AudioSource* FindInstance(int handle);
    void StopInstance(int handle);
    void StopAll();

private:
    bool deviceInitialized = false;
    int nextHandle = 1;
    std::unordered_map<std::string, std::shared_ptr<const SampleData>> sampleCache;
    std::unordered_map<int, std::unique_ptr<AudioSource>> instances;
};
