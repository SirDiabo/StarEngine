#include "AudioCore.h"
#include "Core/StarAsset.h"
#include "Core/CookedFile.h"
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
    for (auto& kv : cookedSoundCache) UnloadSound(kv.second);
    cookedSoundCache.clear();
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

Sound* AudioCore::GetOrLoadCookedSound(const std::string& soundAssetPath) {
    auto it = cookedSoundCache.find(soundAssetPath);
    if (it != cookedSoundCache.end()) return &it->second;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(soundAssetPath, asset) || asset.assetType != "Sound") return nullptr;

    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;

    Sound loaded = LoadSound(resolved.c_str());
    if (loaded.frameCount == 0) return nullptr;

    auto inserted = cookedSoundCache.emplace(soundAssetPath, loaded);
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

Music* AudioCore::GetOrLoadCookedMusic(const std::string& musicAssetPath) {
    StarAsset asset;
    if (!StarAsset::LoadFromFile(musicAssetPath, asset) || asset.assetType != "Sound") return nullptr;

    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    if (resolved.empty() || !FileExists(resolved.c_str())) return nullptr;
    Music loaded = LoadMusicStream(resolved.c_str());
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

void AudioCore::StopAllSounds() {
    for (auto& kv : soundCache) StopSound(kv.second);
    for (auto& kv : cookedSoundCache) StopSound(kv.second);
}