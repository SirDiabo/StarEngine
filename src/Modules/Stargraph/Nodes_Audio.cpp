#include "Nodes_Audio.h"
#include "NodeRegistry.h"
#include "Runtime/AudioCore.h"
#include "Core/AudioBusRegistry.h"
#include "raylib.h"
#include <cstdlib>

namespace {

float ReadFloat(const Node& node, const char* pinName, float fallback) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName && !p.defaultValue.empty()) return p.defaultValue.AsFloat();
    }
    return fallback;
}

std::string ReadText(const Node& node, const char* pinName) {
    for (const Pin& p : node.inputs) {
        if (p.name == pinName) return p.defaultValue;
    }
    return std::string();
}

void WarnUnknownBus(const std::string& bus) {
    TraceLog(LOG_WARNING, "Audio: unknown bus '%s'", bus.c_str());
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
        node->inputs.push_back({"Bus", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Sound", PinType::Object, false, true, 0, "", false, "Sound"});
    };
    playSound.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string path;
        std::string bus;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Path") path = p.defaultValue;
            if (p.name == "Bus") bus = p.defaultValue;
            if (p.name == "Looping") looping = (p.defaultValue == "true" || p.defaultValue == "1");
        }
        float volume = ReadFloat(node, "Volume", 1.0f);
        float pitch = ReadFloat(node, "Pitch", 1.0f);
        int handle = AudioCore::Get().PlayAsset(path, volume, pitch, looping, bus);
        for (Pin& p : node.outputs) {
            if (p.name == "Sound") p.defaultValue = std::to_string(handle);
        }
    };
    NodeRegistry::Get().RegisterNode(playSound);

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
        AudioCore::Get().SetBusVolume(AudioBusRegistry::MasterName(), ReadFloat(node, "Volume", 1.0f));
    };
    NodeRegistry::Get().RegisterNode(setMasterVolume);

    NodeTemplate setBusVolume;
    setBusVolume.type = "Set_Bus_Volume";
    setBusVolume.category = "Audio";
    setBusVolume.moduleName = "Stargraph";
    setBusVolume.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Bus", PinType::String, true, false, 0, "Master"});
        node->inputs.push_back({"Volume", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setBusVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string bus = ReadText(node, "Bus");
        if (!AudioCore::Get().SetBusVolume(bus, ReadFloat(node, "Volume", 1.0f))) WarnUnknownBus(bus);
    };
    NodeRegistry::Get().RegisterNode(setBusVolume);

    NodeTemplate getBusVolume;
    getBusVolume.type = "Get_Bus_Volume";
    getBusVolume.category = "Audio";
    getBusVolume.moduleName = "Stargraph";
    getBusVolume.createPins = [](Node* node) {
        node->inputs.push_back({"Bus", PinType::String, true, false, 0, "Master"});
        node->outputs.push_back({"Volume", PinType::Float, false, true});
    };
    getBusVolume.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string bus = ReadText(node, "Bus");
        float volume = 1.0f;
        if (!AudioCore::Get().GetBusVolume(bus, volume)) WarnUnknownBus(bus);
        for (Pin& p : node.outputs) if (p.name == "Volume") p.defaultValue = std::to_string(volume);
    };
    NodeRegistry::Get().RegisterNode(getBusVolume);

    NodeTemplate setBusMuted;
    setBusMuted.type = "Set_Bus_Muted";
    setBusMuted.category = "Audio";
    setBusMuted.moduleName = "Stargraph";
    setBusMuted.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Bus", PinType::String, true, false, 0, "Master"});
        node->inputs.push_back({"Muted", PinType::Boolean, true, false, 0, "true"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setBusMuted.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string bus = ReadText(node, "Bus");
        std::string muted = ReadText(node, "Muted");
        if (!AudioCore::Get().SetBusMuted(bus, muted == "true" || muted == "1")) WarnUnknownBus(bus);
    };
    NodeRegistry::Get().RegisterNode(setBusMuted);

    NodeTemplate isBusMuted;
    isBusMuted.type = "Is_Bus_Muted";
    isBusMuted.category = "Audio";
    isBusMuted.moduleName = "Stargraph";
    isBusMuted.createPins = [](Node* node) {
        node->inputs.push_back({"Bus", PinType::String, true, false, 0, "Master"});
        node->outputs.push_back({"Muted", PinType::Boolean, false, true});
    };
    isBusMuted.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string bus = ReadText(node, "Bus");
        bool muted = false;
        if (!AudioCore::Get().IsBusMuted(bus, muted)) WarnUnknownBus(bus);
        for (Pin& p : node.outputs) if (p.name == "Muted") p.defaultValue = muted ? "true" : "false";
    };
    NodeRegistry::Get().RegisterNode(isBusMuted);
}
