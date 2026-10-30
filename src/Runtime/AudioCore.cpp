#include "AudioCore.h"
#include "Audio/AudioMixer.h"
#include "raylib.h"
#include <vector>

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

int AudioCore::PlayAsset(const std::string& assetPath, float volume, float pitch, bool looping) {
    std::unique_ptr<AudioSource> source = AudioSourceRegistry::Get().Create(assetPath);
    if (!source) return 0;

    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
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
