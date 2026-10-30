#include "Nodes_Audio.h"
#include "NodeRegistry.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <cstdlib>

namespace {

int ReadHandle(const Node& node, const char* pinName = "Sound") {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName && !p.defaultValue.empty()) return std::atoi(p.defaultValue.c_str());
    }
    return 0;
}

float ReadFloat(const Node& node, const char* pinName, float fallback) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName && !p.defaultValue.empty()) return p.defaultValue.AsFloat();
    }
    return fallback;
}

void AddSoundPins(Node* node, bool withExec) {
    if (withExec) node->inputs.push_back({"In", PinType::Exec, true, false});
    node->inputs.push_back({"Sound", PinType::Object, true, false, 0, "", false, "Sound"});
}

}

void RegisterAudioNodes() {
    NodeTemplate playSound;
    playSound.type = "Play_Sound";
    playSound.category = "Audio";
    playSound.moduleName = "Stargraph";
    playSound.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Path", PinType::String, true, false, 0, "", false, "Sound"});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->inputs.push_back({"Pitch", PinType::Float, true, false, 0, "1.0"});
        node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Sound", PinType::Object, false, true, 0, "", false, "Sound"});
    };
    playSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
            if (p.name == "Looping") looping = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        float volume = ReadFloat(node, "Volume", 1.0f);
        float pitch = ReadFloat(node, "Pitch", 1.0f);
        int handle = AudioCore::Get().PlayAsset(path, volume, pitch, looping);
        for (Pin& p : node.outputs) {
            if (p.name == "Sound") p.defaultValue = std::to_string(handle);
        }
    };
    NodeRegistry::Get().RegisterNode(playSound);

    NodeTemplate stopSound;
    stopSound.type = "Stop_Sound";
    stopSound.category = "Audio";
    stopSound.moduleName = "Stargraph";
    stopSound.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stopSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->Stop();
    };
    NodeRegistry::Get().RegisterNode(stopSound);

    NodeTemplate pauseSound;
    pauseSound.type = "Pause_Sound";
    pauseSound.category = "Audio";
    pauseSound.moduleName = "Stargraph";
    pauseSound.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    pauseSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->Pause();
    };
    NodeRegistry::Get().RegisterNode(pauseSound);

    NodeTemplate resumeSound;
    resumeSound.type = "Resume_Sound";
    resumeSound.category = "Audio";
    resumeSound.moduleName = "Stargraph";
    resumeSound.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    resumeSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->Resume();
    };
    NodeRegistry::Get().RegisterNode(resumeSound);

    NodeTemplate isSoundPlaying;
    isSoundPlaying.type = "Is_Sound_Playing";
    isSoundPlaying.category = "Audio";
    isSoundPlaying.moduleName = "Stargraph";
    isSoundPlaying.createPins = [](Node* node) {
        AddSoundPins(node, false);
        node->outputs.push_back({"Playing", PinType::Boolean, false, true});
    };
    isSoundPlaying.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node));
        bool playing = s && s->IsPlaying();
        for (Pin& p : node.outputs) if (p.name == "Playing") p.defaultValue = playing ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(isSoundPlaying);

    NodeTemplate setSoundVolume;
    setSoundVolume.type = "Set_Sound_Volume";
    setSoundVolume.category = "Audio";
    setSoundVolume.moduleName = "Stargraph";
    setSoundVolume.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->SetVolume(ReadFloat(node, "Volume", 1.0f));
    };
    NodeRegistry::Get().RegisterNode(setSoundVolume);

    NodeTemplate setSoundPitch;
    setSoundPitch.type = "Set_Sound_Pitch";
    setSoundPitch.category = "Audio";
    setSoundPitch.moduleName = "Stargraph";
    setSoundPitch.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->inputs.push_back({"Pitch", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundPitch.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->SetPitch(ReadFloat(node, "Pitch", 1.0f));
    };
    NodeRegistry::Get().RegisterNode(setSoundPitch);

    NodeTemplate setSoundTempo;
    setSoundTempo.type = "Set_Sound_Tempo";
    setSoundTempo.category = "Audio";
    setSoundTempo.moduleName = "Stargraph";
    setSoundTempo.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->inputs.push_back({"Tempo", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundTempo.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node));
        if (s && s->SupportsTempo()) s->SetTempo(ReadFloat(node, "Tempo", 1.0f));
    };
    NodeRegistry::Get().RegisterNode(setSoundTempo);

    NodeTemplate seekSound;
    seekSound.type = "Seek_Sound";
    seekSound.category = "Audio";
    seekSound.moduleName = "Stargraph";
    seekSound.createPins = [](Node* node) {
        AddSoundPins(node, true);
        node->inputs.push_back({"Position", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    seekSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (AudioSource* s = AudioCore::Get().FindInstance(ReadHandle(node))) s->Seek(ReadFloat(node, "Position", 0.0f));
    };
    NodeRegistry::Get().RegisterNode(seekSound);

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
        SetMasterVolume(ReadFloat(node, "Volume", 1.0f));
    };
    NodeRegistry::Get().RegisterNode(setMasterVolume);
}
