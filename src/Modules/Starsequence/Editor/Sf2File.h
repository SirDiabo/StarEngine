#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Starsequence {
namespace Sf2 {

struct Diagnostic {
    bool error = false;
    std::string text;
};

struct Bag {
    uint16_t generator = 0;
    uint16_t modulator = 0;
};

struct Generator {
    uint16_t oper = 0;
    int16_t amount = 0;
};

struct PresetHeader {
    std::string name;
    int program = 0;
    int bank = 0;
    int bag = 0;
};

struct InstrumentHeader {
    std::string name;
    int bag = 0;
};

struct SampleHeader {
    std::string name;
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t startLoop = 0;
    uint32_t endLoop = 0;
    uint32_t sampleRate = 44100;
    int originalPitch = 60;
    int pitchCorrection = 0;
    uint16_t link = 0;
    uint16_t type = 1;
};

struct File {
    std::string path;
    std::string bankName;
    int versionMajor = 0;
    int versionMinor = 0;
    bool compressed = false;

    std::vector<uint8_t> bytes;
    size_t sampleOffset = 0;
    size_t sampleBytes = 0;

    std::vector<PresetHeader> presets;
    std::vector<Bag> presetBags;
    std::vector<Generator> presetGenerators;
    std::vector<InstrumentHeader> instruments;
    std::vector<Bag> instrumentBags;
    std::vector<Generator> instrumentGenerators;
    std::vector<SampleHeader> samples;

    int PresetCount() const { return presets.empty() ? 0 : (int)presets.size() - 1; }
    int SampleCount() const { return samples.empty() ? 0 : (int)samples.size() - 1; }
    bool IsDrumPreset(int index) const;
};

struct Envelope {
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 1.0f;
    float release = 0.05f;
};

struct ResolvedZone {
    int sampleIndex = -1;
    int keyLow = 0;
    int keyHigh = 127;
    int velLow = 1;
    int velHigh = 127;
    int rootKey = 60;
    float tuneCents = 0.0f;
    float gainDb = 0.0f;
    float pan = 0.0f;
    bool pitched = true;
    bool loops = false;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    Envelope envelope;
};

bool ParseSf2(std::vector<uint8_t>&& data, File& out, std::vector<Diagnostic>& diagnostics);
bool LoadSf2File(const std::string& path, File& out, std::vector<Diagnostic>& diagnostics);
bool ResolvePreset(const File& file, int presetIndex, std::vector<ResolvedZone>& zones, std::vector<Diagnostic>& diagnostics);
bool ExtractSample(const File& file, int sampleIndex, std::vector<int16_t>& pcm);
Envelope MostCommonEnvelope(const std::vector<ResolvedZone>& zones);

}
}
