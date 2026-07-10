#include "AudioCore.h"
#include <algorithm>

AudioCore& AudioCore::Get() {
    static AudioCore instance;
    return instance;
}

void AudioCore::InitDevice() {
    if (deviceInitialized) return;
    InitAudioDevice();
    deviceInitialized = true;
}

void AudioCore::CloseDevice() {
    if (!deviceInitialized) return;
    StopAndClearMusic();
    for (auto& kv : soundCache) UnloadSound(kv.second);
    soundCache.clear();
    CloseAudioDevice();
    deviceInitialized = false;
}

Sound* AudioCore::GetOrLoadSound(const std::string& path) {
    auto it = soundCache.find(path);
    if (it != soundCache.end()) return &it->second;

    if (path.empty() || !FileExists(path.c_str())) return nullptr;
    Sound loaded = LoadSound(path.c_str());
    if (loaded.frameCount == 0) return nullptr;

    auto inserted = soundCache.emplace(path, loaded);
    return &inserted.first->second;
}

Music* AudioCore::LoadMusicHandle(const std::string& path) {
    if (path.empty() || !FileExists(path.c_str())) return nullptr;
    Music loaded = LoadMusicStream(path.c_str());
    if (loaded.frameCount == 0) return nullptr;

    Music* music = new Music(loaded);
    activeMusic.push_back(music);
    return music;
}

void AudioCore::UnloadMusicHandle(Music* music) {
    if (!music) return;
    activeMusic.erase(std::remove(activeMusic.begin(), activeMusic.end(), music), activeMusic.end());
    UnloadMusicStream(*music);
    delete music;
}

void AudioCore::TickMusicStreams() {
    for (Music* music : activeMusic) {
        UpdateMusicStream(*music);
    }
}

void AudioCore::StopAndClearMusic() {
    for (Music* music : activeMusic) {
        StopMusicStream(*music);
        UnloadMusicStream(*music);
        delete music;
    }
    activeMusic.clear();
}