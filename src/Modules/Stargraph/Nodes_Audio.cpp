#include "Nodes_Audio.h"
#include "NodeRegistry.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <cstdlib>

void RegisterAudioNodes() {
    NodeTemplate playSound;
    playSound.type = "Play_Sound";
    playSound.category = "Audio";
    playSound.moduleName = "Stargraph";
    playSound.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Sound", PinType::Object, false, true});
    };
    playSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) if (p.name == "Path") path = p.defaultValue;
        Sound* snd = AudioCore::Get().GetOrLoadCookedSound(path);
        if (snd) PlaySound(*snd);
        for (Pin& p : node.outputs) {
            if (p.name == "Sound") {
                char buf[64];
                snprintf(buf, sizeof(buf), "%p", (void*)snd);
                p.defaultValue = buf;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(playSound);

    NodeTemplate stopSound;
    stopSound.type = "Stop_Sound";
    stopSound.category = "Audio";
    stopSound.moduleName = "Stargraph";
    stopSound.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Sound", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stopSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Sound* snd = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Sound" && !p.defaultValue.empty()) snd = (Sound*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        if (snd) StopSound(*snd);
    };
    NodeRegistry::Get().RegisterNode(stopSound);

    NodeTemplate pauseSound;
    pauseSound.type = "Pause_Sound";
    pauseSound.category = "Audio";
    pauseSound.moduleName = "Stargraph";
    pauseSound.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Sound", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    pauseSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Sound* snd = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Sound" && !p.defaultValue.empty()) snd = (Sound*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        if (snd) PauseSound(*snd);
    };
    NodeRegistry::Get().RegisterNode(pauseSound);

    NodeTemplate resumeSound;
    resumeSound.type = "Resume_Sound";
    resumeSound.category = "Audio";
    resumeSound.moduleName = "Stargraph";
    resumeSound.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Sound", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    resumeSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Sound* snd = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Sound" && !p.defaultValue.empty()) snd = (Sound*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        if (snd) ResumeSound(*snd);
    };
    NodeRegistry::Get().RegisterNode(resumeSound);

    NodeTemplate isSoundPlaying;
    isSoundPlaying.type = "Is_Sound_Playing";
    isSoundPlaying.category = "Audio";
    isSoundPlaying.moduleName = "Stargraph";
    isSoundPlaying.createPins = [](Node* node) {
        node->inputs.push_back({"Sound", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Playing", PinType::Boolean, false, true});
    };
    isSoundPlaying.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Sound* snd = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Sound" && !p.defaultValue.empty()) snd = (Sound*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        bool playing = snd ? IsSoundPlaying(*snd) : false;
        for (Pin& p : node.outputs) if (p.name == "Playing") p.defaultValue = playing ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(isSoundPlaying);

    NodeTemplate setMasterVolume;
    setMasterVolume.type = "Set_Master_Volume";
    setMasterVolume.category = "Audio";
    setMasterVolume.moduleName = "Stargraph";
    setMasterVolume.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMasterVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        float volume = 1.0f;
        for (const Pin& p : node.inputs) if (p.name == "Volume") volume = (float)atof(p.defaultValue.c_str());
        SetMasterVolume(volume);
    };
    NodeRegistry::Get().RegisterNode(setMasterVolume);

    NodeTemplate loadMusic;
    loadMusic.type = "Load_Music";
    loadMusic.category = "Audio";
    loadMusic.moduleName = "Stargraph";
    loadMusic.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Music", PinType::Object, false, true});
    };
    loadMusic.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        for (const Pin& p : node.inputs) if (p.name == "Path") path = p.defaultValue;
        Music* music = AudioCore::Get().GetOrLoadCookedMusic(path);
        for (Pin& p : node.outputs) {
            if (p.name == "Music") {
                char buf[64];
                snprintf(buf, sizeof(buf), "%p", (void*)music);
                p.defaultValue = buf;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(loadMusic);

    NodeTemplate unloadMusic;
    unloadMusic.type = "Unload_Music";
    unloadMusic.category = "Audio";
    unloadMusic.moduleName = "Stargraph";
    unloadMusic.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Music", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    unloadMusic.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Music* music = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Music" && !p.defaultValue.empty()) music = (Music*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        AudioCore::Get().UnloadMusicHandle(music);
    };
    NodeRegistry::Get().RegisterNode(unloadMusic);

    NodeTemplate playMusic;
    playMusic.type = "Play_Music";
    playMusic.category = "Audio";
    playMusic.moduleName = "Stargraph";
    playMusic.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Music", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    playMusic.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Music* music = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Music" && !p.defaultValue.empty()) music = (Music*)strtoull(p.defaultValue.c_str(), nullptr, 16);
        }
        if (music) PlayMusicStream(*music);
    };
    NodeRegistry::Get().RegisterNode(playMusic);

    NodeTemplate setMusicVolume;
    setMusicVolume.type = "Set_Music_Volume";
    setMusicVolume.category = "Audio";
    setMusicVolume.moduleName = "Stargraph";
    setMusicVolume.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Music", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMusicVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Music* music = nullptr;
        float volume = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Music" && !p.defaultValue.empty()) music = (Music*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            if (p.name == "Volume") volume = (float)atof(p.defaultValue.c_str());
        }
        if (music) SetMusicVolume(*music, volume);
    };
    NodeRegistry::Get().RegisterNode(setMusicVolume);

    NodeTemplate seekMusic;
    seekMusic.type = "Seek_Music";
    seekMusic.category = "Audio";
    seekMusic.moduleName = "Stargraph";
    seekMusic.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Music", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Position", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    seekMusic.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Music* music = nullptr;
        float position = 0.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Music" && !p.defaultValue.empty()) music = (Music*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            if (p.name == "Position") position = (float)atof(p.defaultValue.c_str());
        }
        if (music) SeekMusicStream(*music, position);
    };
    NodeRegistry::Get().RegisterNode(seekMusic);
}