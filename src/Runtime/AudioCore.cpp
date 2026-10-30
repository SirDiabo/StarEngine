#include "AudioCore.h"
#include "Audio/AudioMixer.h"
#include "Core/AudioBusRegistry.h"
#include "raylib.h"
#include <vector>

AudioCore::AudioCore() {
    for (int i = 0; i < kMaxMixerBuses; i++) {
        busVolume[i] = 1.0f;
        busMuted[i] = false;
    }
}

AudioCore& AudioCore::Get() {
    static AudioCore instance;
    return instance;
}

void AudioCore::InitDevice() {
    if (deviceInitialized) return;
    InitAudioDevice();
    AudioMixer::Get().Start();
    deviceInitialized = true;
}

void AudioCore::CloseDevice() {
    if (!deviceInitialized) return;
    instances.clear();
    AudioMixer::Get().StopAllVoices();
    AudioMixer::Get().Shutdown();
    sampleCache.clear();
    CloseAudioDevice();
    deviceInitialized = false;
}

void AudioCore::Tick(float deltaTime) {
    std::vector<int> finished;
    for (auto& kv : instances) {
        kv.second->Tick(deltaTime);
        if (!kv.second->IsPlaying() && !kv.second->IsPaused()) finished.push_back(kv.first);
    }
    for (int handle : finished) instances.erase(handle);
}

std::shared_ptr<const SampleData> AudioCore::GetSampleData(const std::string& resolvedWavPath) {
    auto it = sampleCache.find(resolvedWavPath);
    if (it != sampleCache.end()) return it->second;

    std::string error;
    std::shared_ptr<SampleData> loaded = WavFile::LoadAll(resolvedWavPath, error);
    if (!loaded) {
        TraceLog(LOG_ERROR, "Audio: could not load '%s': %s", resolvedWavPath.c_str(), error.c_str());
        return nullptr;
    }
    sampleCache[resolvedWavPath] = loaded;
    return loaded;
}

void AudioCore::ClearSampleCache() {
    sampleCache.clear();
}

int AudioCore::PlayAsset(const std::string& assetPath, float volume, float pitch, bool looping, const std::string& busName) {
    std::unique_ptr<AudioSource> source = AudioSourceRegistry::Get().Create(assetPath);
    if (!source) return 0;

    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
    int bus = ResolveBus(source->DefaultBusName(), busName);
    source->SetBus(bus < 0 ? 0 : bus);
    source->Play();
    if (!source->IsPlaying()) return 0;

    int handle = nextHandle++;
    instances[handle] = std::move(source);
    return handle;
}

AudioSource* AudioCore::FindInstance(int handle) {
    auto it = instances.find(handle);
    return it == instances.end() ? nullptr : it->second.get();
}

void AudioCore::StopInstance(int handle) {
    if (AudioSource* source = FindInstance(handle)) source->Stop();
}

void AudioCore::StopAll() {
    for (auto& kv : instances) kv.second->Stop();
    instances.clear();
    AudioMixer::Get().StopAllVoices();
}

void AudioCore::ApplyBus(int index) {
    if (index < 0 || index >= kMaxMixerBuses) return;
    AudioMixer::Get().SetBusGain(index, busMuted[index] ? 0.0f : busVolume[index]);
}

bool AudioCore::SetBusVolume(const std::string& busName, float volume) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    busVolume[index] = volume < 0.0f ? 0.0f : volume;
    ApplyBus(index);
    return true;
}

bool AudioCore::GetBusVolume(const std::string& busName, float& volume) const {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    volume = busVolume[index];
    return true;
}

bool AudioCore::SetBusMuted(const std::string& busName, bool muted) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    busMuted[index] = muted;
    ApplyBus(index);
    return true;
}

bool AudioCore::IsBusMuted(const std::string& busName, bool& muted) const {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    muted = busMuted[index];
    return true;
}

void AudioCore::ResetBuses() {
    for (int i = 0; i < kMaxMixerBuses; i++) {
        busVolume[i] = 1.0f;
        busMuted[i] = false;
        ApplyBus(i);
    }
}

int AudioCore::ResolveBus(const std::string& defaultBusName, const std::string& overrideName) const {
    const AudioBusRegistry& registry = AudioBusRegistry::Get();
    if (!overrideName.empty() && overrideName != AudioBusRegistry::AssetDefaultLabel()) {
        int index = registry.IndexOf(overrideName);
        if (index >= 0) return index;
        TraceLog(LOG_WARNING, "Audio: unknown bus '%s', using the asset default", overrideName.c_str());
    }
    int index = registry.IndexOf(defaultBusName);
    return index < 0 ? 0 : index;
}
