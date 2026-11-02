#include "Sf2Import.h"
#include <algorithm>

namespace Starsequence {
namespace Sf2 {

std::string SanitizeFileName(const std::string& name, const std::string& fallback) {
    std::string result;
    for (unsigned char c : name) {
        if (c < 0x20 || c >= 0x7F) continue;
        if (std::string("<>:\"/\\|?*").find((char)c) != std::string::npos) result.push_back('-');
        else result.push_back((char)c);
    }
    size_t start = result.find_first_not_of(" .");
    if (start == std::string::npos) return fallback;
    size_t end = result.find_last_not_of(" .");
    result = result.substr(start, end - start + 1);
    if (result.size() > 80) result.resize(80);
    return result.empty() ? fallback : result;
}

std::vector<uint8_t> EncodeWav16(const std::vector<int16_t>& pcm, int sampleRate) {
    const uint32_t dataBytes = (uint32_t)(pcm.size() * 2);
    std::vector<uint8_t> out;
    out.reserve(44 + dataBytes);
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((uint8_t)(v >> (8 * i))); };
    auto put16 = [&](uint16_t v) { out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8)); };
    auto putTag = [&](const char* tag) { for (int i = 0; i < 4; i++) out.push_back((uint8_t)tag[i]); };
    putTag("RIFF"); put32(36 + dataBytes); putTag("WAVE");
    putTag("fmt "); put32(16); put16(1); put16(1); put32((uint32_t)sampleRate); put32((uint32_t)sampleRate * 2); put16(2); put16(16);
    putTag("data"); put32(dataBytes);
    for (int16_t s : pcm) put16((uint16_t)s);
    return out;
}

bool BuildSampleSet(const File& file, int presetIndex, SampleWriter& writer, std::map<int, std::string>& sampleCache,
                    SampleSet& out, std::vector<Message>& messages) {
    out = SampleSet();
    std::vector<Diagnostic> diagnostics;
    std::vector<ResolvedZone> resolved;
    bool ok = ResolvePreset(file, presetIndex, resolved, diagnostics);
    for (const Diagnostic& d : diagnostics) messages.push_back({ d.error, d.text });
    if (!ok) return false;

    const std::string presetName = file.presets[(size_t)presetIndex].name;
    out.name = presetName.empty() ? "Preset " + std::to_string(presetIndex) : presetName;

    int failedSamples = 0;
    for (const ResolvedZone& z : resolved) {
        std::string path;
        auto cached = sampleCache.find(z.sampleIndex);
        if (cached != sampleCache.end()) {
            path = cached->second;
        } else {
            std::vector<int16_t> pcm;
            const SampleHeader& header = file.samples[(size_t)z.sampleIndex];
            if (ExtractSample(file, z.sampleIndex, pcm)) {
                std::string name = SanitizeFileName(header.name, "Sample " + std::to_string(z.sampleIndex));
                path = writer.Write(name, pcm, (int)std::clamp<uint32_t>(header.sampleRate, 1000u, 192000u));
            }
            sampleCache[z.sampleIndex] = path;
        }
        if (path.empty()) { failedSamples++; continue; }

        SampleZone zone;
        zone.assetPath = path;
        zone.keyLow = z.keyLow;
        zone.keyHigh = z.keyHigh;
        zone.velLow = z.velLow;
        zone.velHigh = z.velHigh;
        zone.rootKey = z.rootKey;
        zone.tuneCents = z.tuneCents;
        zone.gainDb = z.gainDb;
        zone.pan = z.pan;
        zone.pitched = z.pitched;
        if (z.loops) {
            zone.loop = LoopMode::Region;
            zone.loopStart = z.loopStart;
            zone.loopEnd = z.loopEnd;
        }
        out.zones.push_back(zone);
    }

    if (failedSamples > 0) {
        messages.push_back({ false, std::to_string(failedSamples) + (failedSamples == 1 ? " sample" : " samples") + " in \"" + out.name + "\" could not be extracted and " + (failedSamples == 1 ? "was" : "were") + " skipped" });
    }
    if (out.zones.empty()) {
        messages.push_back({ true, "Nothing could be imported from the preset \"" + out.name + "\"" });
        return false;
    }

    Envelope env = MostCommonEnvelope(resolved);
    out.attack = env.attack;
    out.decay = env.decay;
    out.sustain = env.sustain;
    out.release = env.release;
    return true;
}

}
}
