#include "AudioConfig.h"
#include "AudioBusRegistry.h"
#include "AudioEffectRegistry.h"
#include "AudioAttenuationRegistry.h"
#include "NamedListRegistry.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string AudioConfig::BuildPakText() {
    std::string out = "audio.buses=";
    const std::vector<std::string> custom = AudioBusRegistry::Get().CustomNames();
    for (size_t i = 0; i < custom.size(); i++) {
        if (i > 0) out += ",";
        out += custom[i];
    }
    out += "\n";
    out += AudioEffectRegistry::Get().ToText();
    out += AudioAttenuationRegistry::Get().ToText();
    return out;
}

void AudioConfig::LoadPakText(const std::string& text) {
    std::vector<std::string> buses;
    bool haveBuses = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("audio.buses=", 0) != 0) continue;
        haveBuses = true;
        std::stringstream ss(line.substr(12));
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) buses.push_back(item);
        }
    }
    if (haveBuses) NamedListRegistry::Get().SetList(AudioBusRegistry::Category(), buses);
    AudioEffectRegistry::Get().LoadFromText(text);
    AudioAttenuationRegistry::Get().LoadFromText(text);
}

void AudioConfig::LoadLoose(const std::string& directory) {
    fs::path path = fs::path(directory) / "Config" / "AudioConfig.cfg";
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    LoadPakText(buffer.str());
}
