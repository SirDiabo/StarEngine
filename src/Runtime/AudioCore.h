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

    Music* LoadMusicHandle(const std::string& path);
    void UnloadMusicHandle(Music* music);

    void TickMusicStreams();
    void StopAndClearMusic();

private:
    bool deviceInitialized = false;
    std::unordered_map<std::string, Sound> soundCache;
    std::vector<Music*> activeMusic;
};