#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

class AudioCore {
public:
    static AudioCore& Get();

    void InitDevice();
    void CloseDevice();

    Sound* GetOrLoadSound(const std::string& path);
    Sound* GetOrLoadCookedSound(const std::string& soundAssetPath);

    Music* LoadMusicHandle(const std::string& path);
    Music* GetOrLoadCookedMusic(const std::string& musicAssetPath);
    void UnloadMusicHandle(Music* music);

    void TickMusicStreams();
    void StopAndClearMusic();

private:
    bool deviceInitialized = false;
    std::unordered_map<std::string, Sound> soundCache;
    std::unordered_map<std::string, Sound> cookedSoundCache;
    std::vector<Music*> activeMusic;
};