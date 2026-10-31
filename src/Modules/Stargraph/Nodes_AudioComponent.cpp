#include "Runtime/Components/AudioComponent.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "Core/SafeParse.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <cstdlib>
#include "ObjectPinRef.h"
#include "Nodes_Components.h"

void RegisterAudioComponentNodes() {
    NodeTemplate setSoundPath;
    setSoundPath.type = "Set_Sound_Path";
    setSoundPath.category = "Audio";
    setSoundPath.moduleName = "Stargraph";
    setSoundPath.hideFromContextMenu = true;
    setSoundPath.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Sound Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundPath.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Sound Path") path = p.defaultValue;
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->soundPath = path;
        }
    };
    NodeRegistry::Get().RegisterNode(setSoundPath);

    NodeTemplate setVolume;
    setVolume.type = "Set_Volume";
    setVolume.category = "Audio";
    setVolume.moduleName = "Stargraph";
    setVolume.hideFromContextMenu = true;
    setVolume.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float volume = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Volume" && !p.defaultValue.empty()) volume = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->volume = volume;
        }
    };
    NodeRegistry::Get().RegisterNode(setVolume);

    NodeTemplate setLooping;
    setLooping.type = "Set_Looping";
    setLooping.category = "Audio";
    setLooping.moduleName = "Stargraph";
    setLooping.hideFromContextMenu = true;
    setLooping.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setLooping.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Looping") looping = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->looping = looping;
        }
    };
    NodeRegistry::Get().RegisterNode(setLooping);

    NodeTemplate setPitch;
    setPitch.type = "Set_Pitch";
    setPitch.category = "Audio";
    setPitch.moduleName = "Stargraph";
    setPitch.hideFromContextMenu = true;
    setPitch.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Pitch", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setPitch.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float pitch = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Pitch" && !p.defaultValue.empty()) pitch = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->pitch = pitch;
        }
    };
    NodeRegistry::Get().RegisterNode(setPitch);

    NodeTemplate setTempo;
    setTempo.type = "Set_Tempo";
    setTempo.category = "Audio";
    setTempo.moduleName = "Stargraph";
    setTempo.hideFromContextMenu = true;
    setTempo.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Tempo", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setTempo.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float tempo = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Tempo" && !p.defaultValue.empty()) tempo = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->tempo = tempo;
        }
    };
    NodeRegistry::Get().RegisterNode(setTempo);

    NodeTemplate setPan;
    setPan.type = "Set_Pan";
    setPan.category = "Audio";
    setPan.moduleName = "Stargraph";
    setPan.hideFromContextMenu = true;
    setPan.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Pan", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setPan.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float pan = 0.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
            if (p.name == "Pan" && !p.defaultValue.empty()) pan = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->pan = pan;
        }
    };
    NodeRegistry::Get().RegisterNode(setPan);

    NodeTemplate playAudio;
    playAudio.type = "Play_Audio";
    playAudio.category = "Audio";
    playAudio.moduleName = "Stargraph";
    playAudio.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    playAudio.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->Play();
        }
    };
    NodeRegistry::Get().RegisterNode(playAudio);

    NodeTemplate stopAudio;
    stopAudio.type = "Stop_Audio";
    stopAudio.category = "Audio";
    stopAudio.moduleName = "Stargraph";
    stopAudio.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stopAudio.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->Stop();
        }
    };
    NodeRegistry::Get().RegisterNode(stopAudio);

    NodeTemplate pauseAudio;
    pauseAudio.type = "Pause_Audio";
    pauseAudio.category = "Audio";
    pauseAudio.moduleName = "Stargraph";
    pauseAudio.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    pauseAudio.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->Pause();
        }
    };
    NodeRegistry::Get().RegisterNode(pauseAudio);

    NodeTemplate resumeAudio;
    resumeAudio.type = "Resume_Audio";
    resumeAudio.category = "Audio";
    resumeAudio.moduleName = "Stargraph";
    resumeAudio.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    resumeAudio.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "AudioComponent", ctx);
            }
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->Resume();
        }
    };
    NodeRegistry::Get().RegisterNode(resumeAudio);
}
