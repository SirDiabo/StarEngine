#include "Runtime/Components/AudioComponent.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "Core/SafeParse.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <cstdlib>
#include <functional>
#include "ObjectPinRef.h"
#include "Nodes_Components.h"

namespace {

struct AudioTarget {
    AudioComponent* component = nullptr;
    AudioSource* source = nullptr;
    int handle = 0;

    bool Valid() const { return component != nullptr || source != nullptr; }
};

AudioTarget ResolveTarget(Node& node, ExecutionContext& ctx) {
    AudioTarget target;
    for (const Pin& p : node.inputs) {
        if (p.name != "Target" || p.defaultValue.empty()) continue;
        std::string value = p.defaultValue;
        if (value.find(':') != std::string::npos) {
            target.component = ObjectPinRef::ResolveAs<AudioComponent>(node, value, "AudioTarget", ctx);
        } else {
            target.handle = std::atoi(value.c_str());
            target.source = AudioCore::Get().FindInstance(target.handle);
        }
    }
    return target;
}

float ReadFloat(const Node& node, const char* pinName, float fallback) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName && !p.defaultValue.empty()) return p.defaultValue.AsFloat();
    }
    return fallback;
}

bool ReadBool(const Node& node, const char* pinName, bool fallback) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName && !p.defaultValue.empty()) return p.defaultValue.AsBool();
    }
    return fallback;
}

std::string ReadText(const Node& node, const char* pinName) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName) return p.defaultValue;
    }
    return std::string();
}

void SetParameter(AudioTarget& target, const std::string& path, const AudioParamValue& value) {
    if (target.component) target.component->SetParameter(path, value);
    else if (target.source) target.source->SetParameter(path, value);
}

void AddTargetPin(Node* node) {
    node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioTarget"});
}

void RegisterTargetNode(const char* type,
                        std::function<void(Node*)> extraPins,
                        std::function<void(Node&, AudioTarget&)> run) {
    NodeTemplate t;
    t.type = type;
    t.category = "Audio";
    t.moduleName = "Stargraph";
    t.createPins = [extraPins](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        AddTargetPin(node);
        if (extraPins) extraPins(node);
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    t.execute = [run](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioTarget target = ResolveTarget(node, ctx);
        if (!target.Valid()) return;
        run(node, target);
    };
    NodeRegistry::Get().RegisterNode(t);
}

void RegisterLayerNode(const char* type, const char* leaf, const char* valuePin, PinType valueType, const char* valueDefault) {
    std::string leafName = leaf;
    std::string pinName = valuePin;
    bool isBool = valueType == PinType::Boolean;
    RegisterTargetNode(type,
        [pinName, valueType, valueDefault](Node* node) {
            node->inputs.push_back({"Layer", PinType::String, true, false, 0, ""});
            node->inputs.push_back({pinName, valueType, true, false, 0, valueDefault});
        },
        [leafName, pinName, isBool](Node& node, AudioTarget& target) {
            std::string layer = ReadText(node, "Layer");
            if (layer.empty()) return;
            float number = isBool ? (ReadBool(node, pinName.c_str(), false) ? 1.0f : 0.0f) : ReadFloat(node, pinName.c_str(), 0.0f);
            SetParameter(target, "Layers/" + layer + "/" + leafName, AudioParamValue::FromNumber(number));
        });
}

}

void RegisterAudioComponentNodes() {
    NodeTemplate setSoundPath;
    setSoundPath.type = "Set_Sound_Path";
    setSoundPath.category = "Audio";
    setSoundPath.moduleName = "Stargraph";
    setSoundPath.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "AudioComponent"});
        node->inputs.push_back({"Sound Path", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setSoundPath.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioComponent* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<AudioComponent>(node, p.defaultValue, "AudioComponent", ctx);
            }
        }
        if (target) target->soundPath = ReadText(node, "Sound Path");
    };
    NodeRegistry::Get().RegisterNode(setSoundPath);

    RegisterTargetNode("Set_Volume",
        [](Node* node) { node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"}); },
        [](Node& node, AudioTarget& target) {
            float volume = ReadFloat(node, "Volume", 1.0f);
            if (target.component) target.component->volume = volume;
            else target.source->SetVolume(volume);
        });

    RegisterTargetNode("Set_Looping",
        [](Node* node) { node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"}); },
        [](Node& node, AudioTarget& target) {
            bool looping = ReadBool(node, "Looping", false);
            if (target.component) target.component->looping = looping;
            else target.source->SetLooping(looping);
        });

    RegisterTargetNode("Set_Pitch",
        [](Node* node) { node->inputs.push_back({"Pitch", PinType::Float, true, false, 0, "1.0"}); },
        [](Node& node, AudioTarget& target) {
            float pitch = ReadFloat(node, "Pitch", 1.0f);
            if (target.component) target.component->pitch = pitch;
            else target.source->SetPitch(pitch);
        });

    RegisterTargetNode("Set_Tempo",
        [](Node* node) { node->inputs.push_back({"Tempo", PinType::Float, true, false, 0, "1.0"}); },
        [](Node& node, AudioTarget& target) {
            float tempo = ReadFloat(node, "Tempo", 1.0f);
            if (target.component) target.component->tempo = tempo;
            else if (target.source->SupportsTempo()) target.source->SetTempo(tempo);
        });

    RegisterTargetNode("Set_Pan",
        [](Node* node) { node->inputs.push_back({"Pan", PinType::Float, true, false, 0, "0.0"}); },
        [](Node& node, AudioTarget& target) {
            float pan = ReadFloat(node, "Pan", 0.0f);
            if (target.component) target.component->pan = pan;
            else target.source->SetPan(pan);
        });

    RegisterTargetNode("Play_Audio", nullptr,
        [](Node& node, AudioTarget& target) {
            if (target.component) target.component->Play();
            else target.source->Play();
        });

    RegisterTargetNode("Stop_Audio", nullptr,
        [](Node& node, AudioTarget& target) {
            if (target.component) target.component->Stop();
            else AudioCore::Get().StopInstance(target.handle);
        });

    RegisterTargetNode("Pause_Audio", nullptr,
        [](Node& node, AudioTarget& target) {
            if (target.component) target.component->Pause();
            else target.source->Pause();
        });

    RegisterTargetNode("Resume_Audio", nullptr,
        [](Node& node, AudioTarget& target) {
            if (target.component) target.component->Resume();
            else target.source->Resume();
        });

    RegisterTargetNode("Seek_Audio",
        [](Node* node) { node->inputs.push_back({"Position", PinType::Float, true, false, 0, "0.0"}); },
        [](Node& node, AudioTarget& target) {
            float position = ReadFloat(node, "Position", 0.0f);
            AudioSource* source = target.component ? target.component->GetSource() : target.source;
            if (source) source->Seek(position);
        });

    RegisterTargetNode("Set_Audio_Parameter",
        [](Node* node) {
            node->inputs.push_back({"Path", PinType::String, true, false, 0, ""});
            node->inputs.push_back({"Number", PinType::Float, true, false, 0, "0.0"});
            node->inputs.push_back({"Text", PinType::String, true, false, 0, ""});
        },
        [](Node& node, AudioTarget& target) {
            std::string path = ReadText(node, "Path");
            if (path.empty()) return;
            AudioParamValue value;
            value.number = ReadFloat(node, "Number", 0.0f);
            value.text = ReadText(node, "Text");
            SetParameter(target, path, value);
        });

    RegisterTargetNode("Skip_To_Marker",
        [](Node* node) {
            node->inputs.push_back({"Marker", PinType::String, true, false, 0, ""});
            node->inputs.push_back({"Quantize", PinType::String, true, false, 0, ""});
        },
        [](Node& node, AudioTarget& target) {
            std::string marker = ReadText(node, "Marker");
            if (marker.empty()) return;
            std::string quantize = ReadText(node, "Quantize");
            if (!quantize.empty()) {
                AudioParamValue q;
                q.text = quantize;
                SetParameter(target, "Playback/Quantize", q);
            }
            AudioParamValue value;
            value.text = marker;
            SetParameter(target, "Playback/Section", value);
        });

    RegisterLayerNode("Set_Layer_Mute", "Mute", "Muted", PinType::Boolean, "false");
    RegisterLayerNode("Set_Layer_Solo", "Solo", "Soloed", PinType::Boolean, "false");
    RegisterLayerNode("Set_Layer_Volume", "Volume", "Volume", PinType::Float, "1.0");
    RegisterLayerNode("Set_Layer_Pan", "Pan", "Pan", PinType::Float, "0.0");

    NodeTemplate currentMarker;
    currentMarker.type = "Get_Current_Marker";
    currentMarker.category = "Audio";
    currentMarker.moduleName = "Stargraph";
    currentMarker.createPins = [](Node* node) {
        AddTargetPin(node);
        node->outputs.push_back({"Marker", PinType::String, false, true});
    };
    currentMarker.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioTarget target = ResolveTarget(node, ctx);
        AudioParamValue value;
        if (target.component) target.component->GetParameter("Playback/CurrentMarker", value);
        else if (target.source) target.source->GetParameter("Playback/CurrentMarker", value);
        for (Pin& p : node.outputs) if (p.name == "Marker") p.defaultValue = value.text;
    };
    NodeRegistry::Get().RegisterNode(currentMarker);

    NodeTemplate isAudioPlaying;
    isAudioPlaying.type = "Is_Audio_Playing";
    isAudioPlaying.category = "Audio";
    isAudioPlaying.moduleName = "Stargraph";
    isAudioPlaying.createPins = [](Node* node) {
        AddTargetPin(node);
        node->outputs.push_back({"Playing", PinType::Boolean, false, true});
    };
    isAudioPlaying.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        AudioTarget target = ResolveTarget(node, ctx);
        bool playing = false;
        if (target.component) playing = target.component->IsPlaying();
        else if (target.source) playing = target.source->IsPlaying();
        for (Pin& p : node.outputs) if (p.name == "Playing") p.defaultValue = playing ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(isAudioPlaying);
}
