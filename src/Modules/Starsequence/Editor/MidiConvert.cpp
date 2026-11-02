#include "MidiConvert.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

namespace Starsequence {
namespace Midi {

namespace {

const char* kGeneralMidiNames[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
    "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
    "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
    "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
    "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
    "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
    "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
    "Violin", "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
    "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
    "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
    "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
    "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet",
    "Piccolo", "Flute", "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
    "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
    "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
    "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
    "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
    "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
    "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
    "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
    "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter", "Applause", "Gunshot"
};

constexpr int kDrumChannel = 9;

int Priority(EventType type) {
    switch (type) {
        case EventType::Tempo:
        case EventType::TimeSignature:
        case EventType::Marker:
        case EventType::TrackName: return 0;
        case EventType::ProgramChange: return 1;
        case EventType::ControlChange: return 2;
        case EventType::PitchBend: return 3;
        case EventType::NoteOff:
        case EventType::NoteOn: return 4;
    }
    return 0;
}

struct ProgramChange {
    int64_t tick = 0;
    int program = 0;
};

struct ActiveNote {
    int64_t tick = 0;
    int velocity = 100;
    int instrument = 0;
    int track = 0;
};

struct NoteRecord {
    int64_t tick = 0;
    int64_t length = 1;
    int key = 60;
    int velocity = 100;
    int instrument = 0;
    int track = 0;
    int channel = 0;
};

bool ValidDenominator(int d) {
    return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32;
}

}

const char* GeneralMidiName(int program) {
    return kGeneralMidiNames[std::clamp(program, 0, 127)];
}

bool ConvertToSequence(const File& file, const ConvertOptions& options, const std::string& name,
                       Sequence& out, std::vector<ConvertMessage>& messages, ConvertStats& stats) {
    out = Sequence();
    stats = ConvertStats();
    out.name = name;

    auto warn = [&](const std::string& text) { messages.push_back({ false, text }); };
    auto fail = [&](const std::string& text) { messages.push_back({ true, text }); };

    double scale = 1.0;
    int resolution = 480;
    if (file.smpte) {
        scale = 960.0 / std::max(1.0, file.smpteTicksPerSecond);
    } else if (options.convertTo480 || file.ppq <= 0 || file.ppq > 100000) {
        scale = 480.0 / (double)std::max(1, file.ppq);
    } else {
        resolution = file.ppq;
    }
    out.resolution = resolution;
    auto T = [&](int64_t t) -> int64_t { return (int64_t)std::llround((double)t * scale); };

    std::vector<size_t> order(file.events.size());
    std::iota(order.begin(), order.end(), (size_t)0);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Event& x = file.events[a];
        const Event& y = file.events[b];
        if (x.tick != y.tick) return x.tick < y.tick;
        int px = Priority(x.type), py = Priority(y.type);
        if (px != py) return px < py;
        if (x.track != y.track) return x.track < y.track;
        return x.order < y.order;
    });

    if (file.smpte || !options.importTempo) {
        double bpm = file.smpte ? 120.0 : std::clamp((double)options.fixedBpm, 1.0, 999.0);
        out.tempo.push_back({ 0, bpm });
    } else {
        std::vector<TempoPoint> points;
        for (size_t idx : order) {
            const Event& e = file.events[idx];
            if (e.type != EventType::Tempo || e.a <= 0) continue;
            double bpm = std::clamp(60000000.0 / (double)e.a, 1.0, 999.0);
            int64_t tick = T(e.tick);
            if (!points.empty() && points.back().tick == tick) points.back().bpm = bpm;
            else points.push_back({ tick, bpm });
        }
        if (points.empty() || points.front().tick != 0) points.insert(points.begin(), TempoPoint{ 0, 120.0 });
        std::vector<TempoPoint> cleaned;
        for (const TempoPoint& p : points) {
            if (!cleaned.empty() && std::fabs(cleaned.back().bpm - p.bpm) < 1e-9) continue;
            cleaned.push_back(p);
        }
        out.tempo = cleaned;
    }

    out.meter.push_back({ 0, 4, 4 });
    if (options.importMeter) {
        for (size_t idx : order) {
            const Event& e = file.events[idx];
            if (e.type != EventType::TimeSignature) continue;
            if (e.a < 1 || e.a > 32 || !ValidDenominator(e.b)) {
                warn("The time signature " + std::to_string(e.a) + "/" + std::to_string(e.b) + " is not supported and was ignored");
                continue;
            }
            int64_t tick = T(e.tick);
            EventRef ref = AddMeterPoint(out, tick, e.a, e.b);
            if (ref.Valid() && EventTick(out, ref) != tick) {
                warn("A time signature change at tick " + std::to_string(tick) + " was not on a bar boundary and was moved to the start of its bar");
            }
        }
    }

    if (options.importMarkers) {
        for (size_t idx : order) {
            const Event& e = file.events[idx];
            if (e.type == EventType::Marker && !e.text.empty()) AddMarker(out, T(e.tick), e.text);
        }
    }

    std::array<std::vector<ProgramChange>, 16> programs;
    std::array<int64_t, 16> firstNote;
    firstNote.fill(-1);
    std::array<int, 16> bendCurrent;
    std::array<int, 16> bendAtFirst;
    std::array<int, 16> rpnMsb;
    std::array<int, 16> rpnLsb;
    bendCurrent.fill(2);
    bendAtFirst.fill(2);
    rpnMsb.fill(127);
    rpnLsb.fill(127);
    std::array<bool, 16> channelUsed;
    channelUsed.fill(false);

    for (size_t idx : order) {
        const Event& e = file.events[idx];
        if (e.channel < 0 || e.channel > 15) continue;
        const int ch = e.channel;
        if (e.type == EventType::ProgramChange) {
            programs[(size_t)ch].push_back({ e.tick, e.a });
        } else if (e.type == EventType::NoteOn) {
            if (firstNote[(size_t)ch] < 0) {
                firstNote[(size_t)ch] = e.tick;
                bendAtFirst[(size_t)ch] = bendCurrent[(size_t)ch];
            }
            channelUsed[(size_t)ch] = true;
        } else if (e.type == EventType::ControlChange) {
            if (e.a == 101) rpnMsb[(size_t)ch] = e.b;
            else if (e.a == 100) rpnLsb[(size_t)ch] = e.b;
            else if (e.a == 99 || e.a == 98) { rpnMsb[(size_t)ch] = 127; rpnLsb[(size_t)ch] = 127; }
            else if (e.a == 6 && rpnMsb[(size_t)ch] == 0 && rpnLsb[(size_t)ch] == 0) bendCurrent[(size_t)ch] = std::clamp(e.b, 0, 48);
        }
    }

    auto programAt = [&](int ch, int64_t tick) -> int {
        if (ch == kDrumChannel) return 0;
        const std::vector<ProgramChange>& list = programs[(size_t)ch];
        int program = 0;
        for (const ProgramChange& p : list) {
            if (p.tick > tick) break;
            program = p.program;
        }
        return std::clamp(program, 0, 127);
    };

    int usedChannelCount = 0;
    for (bool used : channelUsed) if (used) usedChannelCount++;

    std::map<std::pair<int, int>, int> instrumentIndex;
    std::vector<int> instrumentChannel;
    bool instrumentOverflowWarned = false;

    auto instrumentFor = [&](int ch, int64_t tick, bool create) -> int {
        int program = programAt(ch, tick);
        std::pair<int, int> key;
        if (options.instrumentMode == InstrumentMode::Single) key = { 0, 0 };
        else if (options.instrumentMode == InstrumentMode::Channel) key = { ch, 0 };
        else key = { ch, program };
        auto found = instrumentIndex.find(key);
        if (found != instrumentIndex.end()) return found->second;
        if (!create) return -1;
        if ((int)out.instruments.size() >= kMaxInstruments) {
            if (!instrumentOverflowWarned) {
                warn("More than " + std::to_string(kMaxInstruments) + " instruments are needed; the extra ones were merged into \"" + out.instruments.back().name + "\"");
                instrumentOverflowWarned = true;
            }
            int last = (int)out.instruments.size() - 1;
            instrumentIndex[key] = last;
            return last;
        }
        std::string base;
        if (options.instrumentMode == InstrumentMode::Single) base = "MIDI Instrument";
        else if (ch == kDrumChannel) base = "Drum Kit";
        else base = GeneralMidiName(program);
        std::string unique = base;
        if (out.FindInstrument(unique) >= 0) unique = base + " (Ch " + std::to_string(ch + 1) + ")";
        Instrument inst;
        inst.name = UniqueInstrumentName(out, unique, -1);
        inst.sets.push_back(SampleSet());
        out.instruments.push_back(inst);
        instrumentChannel.push_back(options.instrumentMode == InstrumentMode::Single ? -1 : ch);
        int index = (int)out.instruments.size() - 1;
        instrumentIndex[key] = index;
        return index;
    };

    std::map<int, std::deque<ActiveNote>> active;
    std::vector<NoteRecord> records;
    int concurrent = 0, maxConcurrent = 0;
    int unmatched = 0;
    int64_t latestEnd = 0;

    auto makeRecord = [&](const ActiveNote& a, int ch, int key, int64_t endRaw) {
        NoteRecord r;
        r.tick = T(a.tick);
        r.length = std::max<int64_t>(1, T(endRaw) - r.tick);
        r.key = key;
        r.velocity = a.velocity;
        r.instrument = a.instrument;
        r.track = a.track;
        r.channel = ch;
        latestEnd = std::max(latestEnd, r.tick + r.length);
        records.push_back(r);
    };

    for (size_t idx : order) {
        const Event& e = file.events[idx];
        if (e.type != EventType::NoteOn && e.type != EventType::NoteOff) continue;
        if (e.channel < 0 || e.channel > 15 || e.a < 0 || e.a > 127) continue;
        const int slot = e.channel * 128 + e.a;
        if (e.type == EventType::NoteOn) {
            ActiveNote a;
            a.tick = e.tick;
            a.velocity = std::clamp(e.b, 1, 127);
            a.instrument = instrumentFor(e.channel, e.tick, true);
            a.track = e.track;
            active[slot].push_back(a);
            concurrent++;
            maxConcurrent = std::max(maxConcurrent, concurrent);
        } else {
            auto it = active.find(slot);
            if (it == active.end() || it->second.empty()) continue;
            ActiveNote a = it->second.front();
            it->second.pop_front();
            concurrent--;
            makeRecord(a, e.channel, e.a, e.tick);
        }
    }
    for (auto& entry : active) {
        for (const ActiveNote& a : entry.second) {
            int64_t trackEnd = a.track >= 0 && (size_t)a.track < file.tracks.size() ? file.tracks[(size_t)a.track].endTick : a.tick;
            makeRecord(a, entry.first / 128, entry.first % 128, std::max(trackEnd, a.tick + 1));
            unmatched++;
        }
    }
    if (unmatched > 0) {
        warn(std::to_string(unmatched) + (unmatched == 1 ? " note has" : " notes have") + " no matching note-off and " + (unmatched == 1 ? "was" : "were") + " ended at the end of its track");
    }

    if (records.empty()) {
        fail("The MIDI file contains no notes");
        return false;
    }

    LayerMode layerMode = options.layerMode;
    auto groupOf = [&](const NoteRecord& r, LayerMode mode) -> std::pair<int, int> {
        if (mode == LayerMode::TrackChannel) return { r.track, r.channel };
        if (mode == LayerMode::Track) return { r.track, -1 };
        return { 0, -1 };
    };
    auto collectGroups = [&](LayerMode mode) {
        std::set<std::pair<int, int>> groups;
        for (const NoteRecord& r : records) groups.insert(groupOf(r, mode));
        return groups;
    };
    std::set<std::pair<int, int>> groups = collectGroups(layerMode);
    if (layerMode == LayerMode::TrackChannel && (int)groups.size() > kMaxLayers) {
        warn("This file has " + std::to_string(groups.size()) + " track and channel combinations, more than the " + std::to_string(kMaxLayers) + " layers allowed; notes were grouped by track instead");
        layerMode = LayerMode::Track;
        groups = collectGroups(layerMode);
    }
    std::map<std::pair<int, int>, int> layerOfGroup;
    {
        int next = 0;
        bool warned = false;
        for (const auto& g : groups) {
            if (next >= kMaxLayers) {
                if (!warned) {
                    warn("More than " + std::to_string(kMaxLayers) + " layers are needed; the extra tracks were merged into the last layer");
                    warned = true;
                }
                layerOfGroup[g] = kMaxLayers - 1;
            } else {
                layerOfGroup[g] = next++;
            }
        }
    }

    std::map<int, int> groupsPerTrack;
    for (const auto& g : groups) groupsPerTrack[g.first]++;
    std::vector<std::string> layerBaseNames((size_t)std::min<int>((int)groups.size(), kMaxLayers));
    for (const auto& g : groups) {
        int layerIndex = layerOfGroup[g];
        if (!layerBaseNames[(size_t)layerIndex].empty()) continue;
        std::string base;
        if (layerMode == LayerMode::Single) {
            base = name.empty() ? std::string("Layer 1") : name;
        } else {
            std::string trackName = (size_t)g.first < file.tracks.size() ? file.tracks[(size_t)g.first].name : std::string();
            base = trackName.empty() ? "Track " + std::to_string(g.first + 1) : trackName;
            if (layerMode == LayerMode::TrackChannel && groupsPerTrack[g.first] > 1) base += " Ch " + std::to_string(g.second + 1);
        }
        layerBaseNames[(size_t)layerIndex] = base;
    }
    std::vector<unsigned> layerChannels(layerBaseNames.size(), 0);
    for (const std::string& base : layerBaseNames) {
        Layer layer;
        layer.name = UniqueLayerName(out, base, -1);
        out.layers.push_back(layer);
    }

    const int64_t quantizeGrid = options.quantizeDenominator > 0 ? std::max<int64_t>(1, (int64_t)resolution * 4 / options.quantizeDenominator) : 0;
    for (const NoteRecord& r : records) {
        int layerIndex = layerOfGroup[groupOf(r, layerMode)];
        Note n;
        n.instrument = r.instrument;
        n.tick = r.tick;
        n.length = r.length;
        n.key = r.key;
        n.velocity = r.velocity;
        if (quantizeGrid > 0) n.tick = ((n.tick + quantizeGrid / 2) / quantizeGrid) * quantizeGrid;
        out.layers[(size_t)layerIndex].notes.push_back(n);
        layerChannels[(size_t)layerIndex] |= 1u << r.channel;
        latestEnd = std::max(latestEnd, n.tick + n.length);
        stats.notes++;
    }
    for (Layer& layer : out.layers) SortNotes(layer);

    for (size_t i = 0; i < out.instruments.size(); i++) {
        int ch = i < instrumentChannel.size() ? instrumentChannel[i] : -1;
        if (ch >= 0) out.instruments[i].sets[0].bendRange = std::clamp(bendAtFirst[(size_t)ch], 0, 48);
    }

    const bool controllersAllowed = !(options.instrumentMode == InstrumentMode::Single && usedChannelCount > 1);
    if (controllersAllowed) {
        std::map<std::tuple<int, int, int>, int> lastValue;
        for (size_t idx : order) {
            const Event& e = file.events[idx];
            if (e.channel < 0 || e.channel > 15) continue;
            const int ch = e.channel;
            if (firstNote[(size_t)ch] < 0) continue;
            ControllerType type;
            int value = 0;
            if (e.type == EventType::PitchBend) {
                if (!options.importBend) continue;
                type = ControllerType::Bend;
                value = std::clamp(e.a - 8192, -8192, 8191);
            } else if (e.type == EventType::ControlChange) {
                switch (e.a) {
                    case 7: if (!options.importVolume) continue; type = ControllerType::Volume; value = e.b; break;
                    case 11: if (!options.importExpression) continue; type = ControllerType::Expression; value = e.b; break;
                    case 10: if (!options.importPan) continue; type = ControllerType::Pan; value = std::clamp(e.b - 64, -64, 63); break;
                    case 64: if (!options.importSustain) continue; type = ControllerType::Sustain; value = e.b; break;
                    case 91: if (!options.importEffects) continue; type = ControllerType::ReverbSend; value = e.b; break;
                    case 93: if (!options.importEffects) continue; type = ControllerType::ChorusSend; value = e.b; break;
                    default: continue;
                }
            } else {
                continue;
            }
            int instrument = instrumentFor(ch, std::max(e.tick, firstNote[(size_t)ch]), false);
            if (instrument < 0) continue;
            for (size_t layerIndex = 0; layerIndex < out.layers.size(); layerIndex++) {
                if (!(layerChannels[layerIndex] & (1u << ch))) continue;
                auto key = std::make_tuple((int)layerIndex, instrument, (int)type);
                auto previous = lastValue.find(key);
                if (previous != lastValue.end() && previous->second == value) continue;
                lastValue[key] = value;
                ControllerEvent c;
                c.type = type;
                c.instrument = instrument;
                c.tick = T(e.tick);
                c.value = value;
                out.layers[layerIndex].controllers.push_back(c);
                stats.controllers++;
            }
        }
    }

    int64_t endTick = latestEnd;
    for (const TrackInfo& t : file.tracks) endTick = std::max(endTick, T(t.endTick));
    int64_t barStart = BarStartTick(out, endTick);
    int64_t length = barStart < endTick ? barStart + std::max<int64_t>(1, out.BarTicksAt(barStart)) : barStart;
    out.length = std::max<int64_t>(length, out.BarTicksAt(0));

    out.polyphony = std::clamp(std::max(64, maxConcurrent + 8), 64, kMaxPolyphony);
    out.Normalize();
    stats.layers = (int)out.layers.size();
    stats.instruments = (int)out.instruments.size();
    return true;
}

}
}
