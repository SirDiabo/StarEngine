#include "Sf2File.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <tuple>

namespace Starsequence {
namespace Sf2 {

namespace {

constexpr uintmax_t kMaxSf2Size = 1024ull * 1024ull * 1024ull;
constexpr size_t kMaxZonesPerPreset = 4096;

enum GeneratorId {
    kStartAddrsOffset = 0, kEndAddrsOffset = 1, kStartLoopAddrsOffset = 2, kEndLoopAddrsOffset = 3,
    kStartAddrsCoarseOffset = 4, kEndAddrsCoarseOffset = 12, kPan = 17,
    kAttackVolEnv = 34, kDecayVolEnv = 36, kSustainVolEnv = 37, kReleaseVolEnv = 38,
    kInstrument = 41, kKeyRange = 43, kVelRange = 44, kStartLoopAddrsCoarseOffset = 45,
    kInitialAttenuation = 48, kEndLoopAddrsCoarseOffset = 50, kCoarseTune = 51, kFineTune = 52,
    kSampleId = 53, kSampleModes = 54, kScaleTuning = 56, kOverridingRootKey = 58, kGeneratorCount = 64
};

uint32_t Le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t Le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
bool Tag(const uint8_t* p, const char* tag) { return std::memcmp(p, tag, 4) == 0; }

std::string FixedName(const uint8_t* p, size_t length) {
    std::string name;
    for (size_t i = 0; i < length && p[i] != 0; i++) {
        unsigned char c = p[i];
        if (c < 0x20 || c == 0x7F) continue;
        name.push_back(c >= 0x80 ? '?' : (char)c);
    }
    size_t start = name.find_first_not_of(' ');
    if (start == std::string::npos) return std::string();
    size_t end = name.find_last_not_of(' ');
    return name.substr(start, end - start + 1);
}

struct ChunkView {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t offset = 0;
};

struct GeneratorSet {
    bool has[kGeneratorCount] = {};
    int16_t value[kGeneratorCount] = {};

    void Apply(const std::vector<Generator>& list, int first, int last) {
        for (int i = first; i < last && i < (int)list.size(); i++) {
            uint16_t oper = list[(size_t)i].oper;
            if (oper >= kGeneratorCount) continue;
            has[oper] = true;
            value[oper] = list[(size_t)i].amount;
        }
    }

    int Get(int oper, int fallback) const { return has[oper] ? value[oper] : fallback; }
};

void Range(const GeneratorSet& g, int oper, int& lo, int& hi) {
    lo = 0;
    hi = 127;
    if (!g.has[oper]) return;
    uint16_t raw = (uint16_t)g.value[oper];
    lo = raw & 0xFF;
    hi = (raw >> 8) & 0xFF;
}

float TimecentsToSeconds(int timecents) {
    if (timecents <= -12000) return 0.0f;
    return (float)std::min(20.0, std::pow(2.0, (double)timecents / 1200.0));
}

}

bool File::IsDrumPreset(int index) const {
    if (index < 0 || index >= PresetCount()) return false;
    return presets[(size_t)index].bank == 128;
}

bool ParseSf2(std::vector<uint8_t>&& data, File& out, std::vector<Diagnostic>& diagnostics) {
    out = File();
    out.bytes = std::move(data);
    const uint8_t* base = out.bytes.data();
    const size_t size = out.bytes.size();

    auto fail = [&](const std::string& text) {
        diagnostics.push_back({ true, text });
        return false;
    };

    if (size < 12 || !Tag(base, "RIFF") || !Tag(base + 8, "sfbk")) return fail("This is not a SoundFont 2 file");

    ChunkView phdr, pbag, pgen, inst, ibag, igen, shdr;
    bool havePdta = false, haveSdta = false;
    size_t pos = 12;
    size_t riffEnd = std::min<size_t>(size, 8 + (size_t)Le32(base + 4));

    while (pos + 8 <= riffEnd) {
        uint32_t chunkSize = Le32(base + pos + 4);
        size_t dataStart = pos + 8;
        size_t available = riffEnd - dataStart;
        size_t length = std::min<size_t>(chunkSize, available);
        if (Tag(base + pos, "LIST") && length >= 4) {
            const uint8_t* listType = base + dataStart;
            size_t sub = dataStart + 4;
            size_t listEnd = dataStart + length;
            bool isInfo = Tag(listType, "INFO"), isSdta = Tag(listType, "sdta"), isPdta = Tag(listType, "pdta");
            while (sub + 8 <= listEnd) {
                uint32_t subSize = Le32(base + sub + 4);
                size_t subData = sub + 8;
                size_t subLength = std::min<size_t>(subSize, listEnd - subData);
                const uint8_t* id = base + sub;
                ChunkView view{ base + subData, subLength, subData };
                if (isInfo) {
                    if (Tag(id, "ifil") && subLength >= 4) { out.versionMajor = Le16(base + subData); out.versionMinor = Le16(base + subData + 2); }
                    else if (Tag(id, "INAM")) out.bankName = FixedName(base + subData, subLength);
                } else if (isSdta) {
                    if (Tag(id, "smpl")) { out.sampleOffset = subData; out.sampleBytes = subLength; haveSdta = true; }
                } else if (isPdta) {
                    if (Tag(id, "phdr")) phdr = view;
                    else if (Tag(id, "pbag")) pbag = view;
                    else if (Tag(id, "pgen")) pgen = view;
                    else if (Tag(id, "inst")) inst = view;
                    else if (Tag(id, "ibag")) ibag = view;
                    else if (Tag(id, "igen")) igen = view;
                    else if (Tag(id, "shdr")) shdr = view;
                    if (Tag(id, "phdr")) havePdta = true;
                }
                size_t next = subData + subLength + (subLength & 1);
                if (next <= sub) break;
                sub = next;
            }
        }
        size_t next = dataStart + length + (length & 1);
        if (next <= pos) break;
        pos = next;
    }

    if (!havePdta || !phdr.data || !pbag.data || !pgen.data || !inst.data || !ibag.data || !igen.data || !shdr.data) {
        return fail("The SoundFont is missing required preset or sample tables");
    }
    if (!haveSdta) return fail("The SoundFont contains no sample data");
    if (phdr.size % 38 || pbag.size % 4 || pgen.size % 4 || inst.size % 22 || ibag.size % 4 || igen.size % 4 || shdr.size % 46) {
        return fail("The SoundFont tables are damaged (unexpected record sizes)");
    }

    for (size_t i = 0; i + 38 <= phdr.size; i += 38) {
        const uint8_t* r = phdr.data + i;
        PresetHeader p;
        p.name = FixedName(r, 20);
        p.program = Le16(r + 20);
        p.bank = Le16(r + 22);
        p.bag = Le16(r + 24);
        out.presets.push_back(p);
    }
    for (size_t i = 0; i + 4 <= pbag.size; i += 4) out.presetBags.push_back({ Le16(pbag.data + i), Le16(pbag.data + i + 2) });
    for (size_t i = 0; i + 4 <= pgen.size; i += 4) out.presetGenerators.push_back({ Le16(pgen.data + i), (int16_t)Le16(pgen.data + i + 2) });
    for (size_t i = 0; i + 22 <= inst.size; i += 22) {
        InstrumentHeader h;
        h.name = FixedName(inst.data + i, 20);
        h.bag = Le16(inst.data + i + 20);
        out.instruments.push_back(h);
    }
    for (size_t i = 0; i + 4 <= ibag.size; i += 4) out.instrumentBags.push_back({ Le16(ibag.data + i), Le16(ibag.data + i + 2) });
    for (size_t i = 0; i + 4 <= igen.size; i += 4) out.instrumentGenerators.push_back({ Le16(igen.data + i), (int16_t)Le16(igen.data + i + 2) });
    for (size_t i = 0; i + 46 <= shdr.size; i += 46) {
        const uint8_t* r = shdr.data + i;
        SampleHeader s;
        s.name = FixedName(r, 20);
        s.start = Le32(r + 20);
        s.end = Le32(r + 24);
        s.startLoop = Le32(r + 28);
        s.endLoop = Le32(r + 32);
        s.sampleRate = Le32(r + 36);
        s.originalPitch = r[40];
        s.pitchCorrection = (int8_t)r[41];
        s.link = Le16(r + 42);
        s.type = Le16(r + 44);
        if (s.type & 0x10) out.compressed = true;
        out.samples.push_back(s);
    }

    if (out.presets.size() < 2) return fail("The SoundFont contains no presets");
    if (out.compressed || out.versionMajor >= 3) {
        diagnostics.push_back({ true, "Compressed SoundFonts (.sf3) are not supported yet; use an uncompressed .sf2" });
        return false;
    }
    return true;
}

bool LoadSf2File(const std::string& path, File& out, std::vector<Diagnostic>& diagnostics) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        diagnostics.push_back({ true, "Could not open " + path });
        return false;
    }
    std::streamoff length = in.tellg();
    if (length <= 0 || (uintmax_t)length > kMaxSf2Size) {
        diagnostics.push_back({ true, "The SoundFont is empty or larger than 1 GB" });
        return false;
    }
    std::vector<uint8_t> bytes((size_t)length);
    in.seekg(0);
    in.read((char*)bytes.data(), length);
    if (!in) {
        diagnostics.push_back({ true, "Could not read " + path });
        return false;
    }
    bool ok = ParseSf2(std::move(bytes), out, diagnostics);
    out.path = path;
    return ok;
}

bool ExtractSample(const File& file, int sampleIndex, std::vector<int16_t>& pcm) {
    pcm.clear();
    if (sampleIndex < 0 || sampleIndex >= file.SampleCount()) return false;
    const SampleHeader& s = file.samples[(size_t)sampleIndex];
    if (s.type & 0x8000) return false;
    if (s.end <= s.start) return false;
    uint64_t totalFrames = file.sampleBytes / 2;
    if ((uint64_t)s.end > totalFrames) return false;
    size_t frames = (size_t)(s.end - s.start);
    if (frames < 2) return false;
    pcm.resize(frames);
    const uint8_t* src = file.bytes.data() + file.sampleOffset + (size_t)s.start * 2;
    for (size_t i = 0; i < frames; i++) pcm[i] = (int16_t)Le16(src + i * 2);
    return true;
}

bool ResolvePreset(const File& file, int presetIndex, std::vector<ResolvedZone>& zones, std::vector<Diagnostic>& diagnostics) {
    zones.clear();
    if (presetIndex < 0 || presetIndex >= file.PresetCount()) {
        diagnostics.push_back({ true, "The selected preset does not exist" });
        return false;
    }
    const PresetHeader& preset = file.presets[(size_t)presetIndex];
    const int bagFirst = preset.bag;
    const int bagLast = file.presets[(size_t)presetIndex + 1].bag;
    const std::string presetName = preset.name.empty() ? "Preset " + std::to_string(presetIndex) : preset.name;
    if (bagFirst < 0 || bagLast < bagFirst || (size_t)bagLast + 1 > file.presetBags.size()) {
        diagnostics.push_back({ true, "The preset \"" + presetName + "\" has damaged zone data" });
        return false;
    }

    GeneratorSet presetGlobal;
    int skippedRom = 0, skippedInvalid = 0;

    for (int pz = bagFirst; pz < bagLast; pz++) {
        int g0 = file.presetBags[(size_t)pz].generator;
        int g1 = file.presetBags[(size_t)pz + 1].generator;
        if (g1 < g0 || (size_t)g1 > file.presetGenerators.size()) { skippedInvalid++; continue; }
        GeneratorSet presetZone;
        presetZone.Apply(file.presetGenerators, g0, g1);
        if (!presetZone.has[kInstrument]) {
            if (pz == bagFirst) presetGlobal = presetZone;
            continue;
        }
        GeneratorSet presetLevel = presetGlobal;
        for (int k = 0; k < kGeneratorCount; k++) if (presetZone.has[k]) { presetLevel.has[k] = true; presetLevel.value[k] = presetZone.value[k]; }

        int instrumentIndex = presetZone.value[kInstrument];
        if (instrumentIndex < 0 || instrumentIndex >= (int)file.instruments.size() - 1) { skippedInvalid++; continue; }
        int ibFirst = file.instruments[(size_t)instrumentIndex].bag;
        int ibLast = file.instruments[(size_t)instrumentIndex + 1].bag;
        if (ibFirst < 0 || ibLast < ibFirst || (size_t)ibLast + 1 > file.instrumentBags.size()) { skippedInvalid++; continue; }

        GeneratorSet instrumentGlobal;
        for (int iz = ibFirst; iz < ibLast; iz++) {
            int i0 = file.instrumentBags[(size_t)iz].generator;
            int i1 = file.instrumentBags[(size_t)iz + 1].generator;
            if (i1 < i0 || (size_t)i1 > file.instrumentGenerators.size()) { skippedInvalid++; continue; }
            GeneratorSet instrumentZone;
            instrumentZone.Apply(file.instrumentGenerators, i0, i1);
            if (!instrumentZone.has[kSampleId]) {
                if (iz == ibFirst) instrumentGlobal = instrumentZone;
                continue;
            }
            GeneratorSet g = instrumentGlobal;
            for (int k = 0; k < kGeneratorCount; k++) if (instrumentZone.has[k]) { g.has[k] = true; g.value[k] = instrumentZone.value[k]; }

            int sampleIndex = g.value[kSampleId];
            if (sampleIndex < 0 || sampleIndex >= file.SampleCount()) { skippedInvalid++; continue; }
            const SampleHeader& sh = file.samples[(size_t)sampleIndex];
            if (sh.type & 0x8000) { skippedRom++; continue; }

            int keyLo, keyHi, velLo, velHi, pKeyLo, pKeyHi, pVelLo, pVelHi;
            Range(g, kKeyRange, keyLo, keyHi);
            Range(g, kVelRange, velLo, velHi);
            Range(presetLevel, kKeyRange, pKeyLo, pKeyHi);
            Range(presetLevel, kVelRange, pVelLo, pVelHi);
            keyLo = std::max(keyLo, pKeyLo);
            keyHi = std::min(keyHi, pKeyHi);
            velLo = std::max(velLo, pVelLo);
            velHi = std::min(velHi, pVelHi);
            if (keyLo > keyHi || velLo > velHi) continue;

            auto sum = [&](int oper, int fallback) { return g.Get(oper, fallback) + presetLevel.Get(oper, 0); };

            ResolvedZone z;
            z.sampleIndex = sampleIndex;
            z.keyLow = std::clamp(keyLo, 0, 127);
            z.keyHigh = std::clamp(keyHi, 0, 127);
            z.velLow = std::clamp(std::max(velLo, 1), 1, 127);
            z.velHigh = std::clamp(velHi, 1, 127);
            if (z.velLow > z.velHigh) continue;

            int root = g.Get(kOverridingRootKey, -1);
            if (root < 0 || root > 127) root = sh.originalPitch <= 127 ? sh.originalPitch : 60;
            z.rootKey = root;
            z.tuneCents = (float)(sum(kCoarseTune, 0) * 100 + sum(kFineTune, 0) + sh.pitchCorrection);
            z.gainDb = std::clamp(-(float)std::max(0, sum(kInitialAttenuation, 0)) * 0.04f, -48.0f, 24.0f);
            z.pan = std::clamp((float)sum(kPan, 0) / 500.0f, -1.0f, 1.0f);
            if (z.pan == 0.0f) {
                if ((sh.type & 0x7FFF) == 4) z.pan = -1.0f;
                else if ((sh.type & 0x7FFF) == 2) z.pan = 1.0f;
            }
            z.pitched = sum(kScaleTuning, 100) != 0;

            int mode = g.Get(kSampleModes, 0) & 3;
            if (mode == 1 || mode == 3) {
                int64_t startLoop = (int64_t)sh.startLoop + g.Get(kStartLoopAddrsOffset, 0) + 32768LL * g.Get(kStartLoopAddrsCoarseOffset, 0) - (int64_t)sh.start;
                int64_t endLoop = (int64_t)sh.endLoop + g.Get(kEndLoopAddrsOffset, 0) + 32768LL * g.Get(kEndLoopAddrsCoarseOffset, 0) - (int64_t)sh.start;
                int64_t length = (int64_t)sh.end - (int64_t)sh.start;
                if (startLoop >= 0 && endLoop > startLoop && endLoop <= length) {
                    z.loops = true;
                    z.loopStart = (uint32_t)startLoop;
                    z.loopEnd = (uint32_t)endLoop;
                }
            }

            z.envelope.attack = TimecentsToSeconds(sum(kAttackVolEnv, -12000));
            z.envelope.decay = TimecentsToSeconds(sum(kDecayVolEnv, -12000));
            int sustainCb = std::clamp(sum(kSustainVolEnv, 0), 0, 1440);
            z.envelope.sustain = (float)std::pow(10.0, -(double)sustainCb / 200.0);
            z.envelope.release = std::max(0.002f, TimecentsToSeconds(sum(kReleaseVolEnv, -12000)));

            zones.push_back(z);
            if (zones.size() >= kMaxZonesPerPreset) {
                diagnostics.push_back({ false, "The preset \"" + presetName + "\" has more than " + std::to_string(kMaxZonesPerPreset) + " zones; the rest were skipped" });
                return true;
            }
        }
    }

    if (skippedRom > 0) diagnostics.push_back({ false, std::to_string(skippedRom) + " ROM sample reference(s) in \"" + presetName + "\" were skipped" });
    if (skippedInvalid > 0) diagnostics.push_back({ false, std::to_string(skippedInvalid) + " damaged zone(s) in \"" + presetName + "\" were skipped" });
    if (zones.empty()) {
        diagnostics.push_back({ true, "The preset \"" + presetName + "\" contains no usable samples" });
        return false;
    }
    return true;
}

Envelope MostCommonEnvelope(const std::vector<ResolvedZone>& zones) {
    if (zones.empty()) return Envelope();
    using Key = std::tuple<int, int, int, int>;
    std::map<Key, int> counts;
    std::vector<Key> order;
    std::map<Key, Envelope> samples;
    for (const ResolvedZone& z : zones) {
        Key key{ (int)std::lround(z.envelope.attack * 1000.0f), (int)std::lround(z.envelope.decay * 1000.0f),
                 (int)std::lround(z.envelope.sustain * 1000.0f), (int)std::lround(z.envelope.release * 1000.0f) };
        if (!counts.count(key)) { order.push_back(key); samples[key] = z.envelope; }
        counts[key]++;
    }
    Key best = order.front();
    for (const Key& k : order) if (counts[k] > counts[best]) best = k;
    return samples[best];
}

}
}
