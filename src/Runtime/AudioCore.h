#pragma once
#include "Audio/AudioMixer.h"
#include "Audio/AudioSource.h"
#include "Audio/WavFile.h"
#include <memory>
#include <string>
#include <unordered_map>

class AudioCore {
public:
    AudioCore();

    static AudioCore& Get();

    void InitDevice();
    void CloseDevice();

    void Tick(float deltaTime);

    std::shared_ptr<const SampleData> GetSampleData(const std::string& resolvedWavPath);
    void ClearSampleCache();

    int PlayAsset(const std::string& assetPath, float volume = 1.0f, float pitch = 1.0f, bool looping = false, const std::string& busName = std::string());
    AudioSource* FindInstance(int handle);
    void StopInstance(int handle);
    void StopAll();

    bool SetBusVolume(const std::string& busName, float volume);
    bool GetBusVolume(const std::string& busName, float& volume) const;
    bool SetBusMuted(const std::string& busName, bool muted);
    bool IsBusMuted(const std::string& busName, bool& muted) const;
    void ResetBuses();
    int ResolveBus(const std::string& defaultBusName, const std::string& overrideName) const;

private:
    void ApplyBus(int index);

    float busVolume[kMaxMixerBuses];
    bool busMuted[kMaxMixerBuses];

    bool deviceInitialized = false;
    int nextHandle = 1;
    std::unordered_map<std::string, std::shared_ptr<const SampleData>> sampleCache;
    std::unordered_map<int, std::unique_ptr<AudioSource>> instances;
};
