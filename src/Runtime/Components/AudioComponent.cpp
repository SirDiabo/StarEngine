#include "AudioComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include <cstdlib>

void RegisterAudioComponentNodes() {
    NodeTemplate setSoundPath;
    setSoundPath.type = "Set_Sound_Path";
    setSoundPath.category = "Audio";
    setSoundPath.moduleName = "Stargraph";
    setSoundPath.hideFromContextMenu = true;
    setSoundPath.requiresComponentClass = "AudioComponent";
    setSoundPath.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Sound Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundPath.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
    setVolume.requiresComponentClass = "AudioComponent";
    setVolume.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float volume = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
    setLooping.requiresComponentClass = "AudioComponent";
    setLooping.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setLooping.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Looping") looping = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        if (!target) return;
        if (AudioComponent* ac = dynamic_cast<AudioComponent*>(target)) {
            ac->looping = looping;
        }
    };
    NodeRegistry::Get().RegisterNode(setLooping);
}

void AudioComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Sound Path", &soundPath);
    outProperties.AddBool("Auto Play", &autoPlay);
    outProperties.AddBool("Looping", &looping);
    outProperties.AddFloat("Volume", &volume);
}

void AudioComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}