#pragma once
#include "Audio/AudioEffects.h"
#include "Audio/AudioEmitter.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioSource.h"
#include "Audio/WavFile.h"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

    bool SetBusEffects(const std::string& busName, const std::vector<std::string>& effectNames);
    bool AddBusEffect(const std::string& busName, const std::string& effectName);
    bool RemoveBusEffect(const std::string& busName, const std::string& effectName);
    bool PopBusEffect(const std::string& busName);
    bool ClearBusEffects(const std::string& busName);
    bool SetBusEffectParameter(const std::string& busName, const std::string& effectName, const std::string& paramId, float value);
    std::vector<std::string> GetBusEffects(const std::string& busName) const;
    void ApplyProjectEffects();

    std::shared_ptr<const AudioEffectChain> BuildEffectChain(const std::vector<std::string>& effectNames, const std::shared_ptr<const AudioEffectChain>& reuseFrom = nullptr);
    void RetireEffectChain(const std::shared_ptr<const AudioEffectChain>& chain);
    static bool SetChainEffectParameter(const AudioEffectChain& chain, const std::string& effectName, const std::string& paramId, float value);
    void SetListener(const Vector3& position, const Vector3& right, Actor* actor);
    void ClearListener();
    void SetRaycastHook(AudioRaycastFn hook);
    int CreateEmitter(AudioSource* source, const std::string& attenuationName);
    void DestroyEmitter(int id, bool resetSource = true);
    AudioEmitter* FindEmitter(int id);
    int PlayAssetSpatial(const std::string& assetPath, float volume, float pitch, bool looping, const std::string& busName,
                         const std::string& attenuationName, AudioPositionFn provider);
    bool SetInstanceAttenuationEnabled(int handle, bool enabled);
    bool SetInstanceAttenuationParameter(int handle, const std::string& paramId, float value);
    bool SetInstanceAttenuation(int handle, const std::string& attenuationName);
    bool SetInstancePosition(int handle, const Vector3& position);
    void OnAttenuationChanged();
    void RefreshEmitter(int id);
    bool PushInstanceEffect(int handle, const std::string& effectName);
    bool PopInstanceEffect(int handle, const std::string& effectName);
    bool ClearInstanceEffects(int handle);
    bool SetInstanceEffectParameter(int handle, const std::string& effectName, const std::string& paramId, float value);
    void OnEffectParamChanged(const std::string& effectName, const std::string& paramId, float value);

private:
    void ApplyBus(int index);
    bool SetBusEffectsByIndex(int index, const std::vector<std::string>& effectNames);
    void PruneRetired();

    struct RetiredChain {
        std::shared_ptr<const AudioEffectChain> chain;
        std::chrono::steady_clock::time_point since;
    };

    std::shared_ptr<const AudioEffectChain> busChains[kMaxMixerBuses];
    std::vector<std::weak_ptr<const AudioEffectChain>> trackedChains;
    std::vector<RetiredChain> retiredChains;

    float busVolume[kMaxMixerBuses];
    bool busMuted[kMaxMixerBuses];

    bool deviceInitialized = false;
    int nextHandle = 1;
    std::unordered_map<std::string, std::shared_ptr<const SampleData>> sampleCache;
    std::unordered_map<int, std::unique_ptr<AudioSource>> instances;

    struct InstanceEffects {
        std::vector<std::string> names;
        std::shared_ptr<const AudioEffectChain> chain;
    };
    std::unordered_map<int, InstanceEffects> instanceEffects;
    std::unordered_map<int, std::unique_ptr<AudioEmitter>> emitters;
    std::unordered_map<int, int> instanceEmitters;
    int nextEmitterId = 1;
    AudioListenerState listener;
    AudioRaycastFn raycastHook;
    void UpdateEmitters(float deltaTime);
    bool ApplyInstanceEffects(int handle, InstanceEffects& entry);
};
