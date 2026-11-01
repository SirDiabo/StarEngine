#include "SequenceEdit.h"
#include <algorithm>
#include <cmath>

namespace Starsequence {

namespace {

bool NameTaken(const std::vector<std::string>& names, const std::string& candidate, int ignoreIndex) {
    for (size_t i = 0; i < names.size(); i++) {
        if ((int)i == ignoreIndex) continue;
        if (names[i] == candidate) return true;
    }
    return false;
}

std::string UniqueName(const std::vector<std::string>& names, std::string base, int ignoreIndex) {
    std::replace(base.begin(), base.end(), '/', '-');
    while (!base.empty() && (base.back() == ' ' || base.back() == '\t')) base.pop_back();
    size_t start = 0;
    while (start < base.size() && (base[start] == ' ' || base[start] == '\t')) start++;
    base = base.substr(start);
    if (base.empty()) base = "Unnamed";
    if (!NameTaken(names, base, ignoreIndex)) return base;
    for (int n = 2; n < 100000; n++) {
        std::string candidate = base + " " + std::to_string(n);
        if (!NameTaken(names, candidate, ignoreIndex)) return candidate;
    }
    return base;
}

std::vector<std::string> LayerNames(const Sequence& s) {
    std::vector<std::string> names;
    for (const Layer& l : s.layers) names.push_back(l.name);
    return names;
}

std::vector<std::string> InstrumentNames(const Sequence& s) {
    std::vector<std::string> names;
    for (const Instrument& i : s.instruments) names.push_back(i.name);
    return names;
}

void ClampNote(Sequence& sequence, Note& n) {
    n.tick = std::max<int64_t>(0, n.tick);
    n.length = std::max<int64_t>(1, n.length);
    n.key = std::clamp(n.key, 0, 127);
    n.velocity = std::clamp(n.velocity, 1, 127);
    int maxInstrument = std::max(0, (int)sequence.instruments.size() - 1);
    n.instrument = std::clamp(n.instrument, 0, maxInstrument);
}

}

void SortNotes(Layer& layer) {
    std::stable_sort(layer.notes.begin(), layer.notes.end(), [](const Note& a, const Note& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        if (a.instrument != b.instrument) return a.instrument < b.instrument;
        return a.key < b.key;
    });
}

void AssignNoteUids(Sequence& sequence, uint32_t& next) {
    for (Layer& layer : sequence.layers) {
        for (Note& n : layer.notes) n.uid = next++;
    }
}

Note* FindNote(Layer& layer, uint32_t uid) {
    for (Note& n : layer.notes) if (n.uid == uid) return &n;
    return nullptr;
}

const Note* FindNote(const Layer& layer, uint32_t uid) {
    for (const Note& n : layer.notes) if (n.uid == uid) return &n;
    return nullptr;
}

uint32_t AddNote(Sequence& sequence, int layerIndex, Note note, uint32_t& next) {
    if (layerIndex < 0 || (size_t)layerIndex >= sequence.layers.size()) return 0;
    ClampNote(sequence, note);
    note.uid = next++;
    Layer& layer = sequence.layers[(size_t)layerIndex];
    layer.notes.push_back(note);
    SortNotes(layer);
    return note.uid;
}

int DeleteNotes(Layer& layer, const std::set<uint32_t>& uids) {
    size_t before = layer.notes.size();
    layer.notes.erase(std::remove_if(layer.notes.begin(), layer.notes.end(),
        [&](const Note& n) { return uids.count(n.uid) > 0; }), layer.notes.end());
    return (int)(before - layer.notes.size());
}

std::vector<NoteSnapshot> SnapshotNotes(const Layer& layer, const std::set<uint32_t>& uids) {
    std::vector<NoteSnapshot> result;
    for (const Note& n : layer.notes) {
        if (uids.count(n.uid)) result.push_back({ n.uid, n.tick, n.length, n.key, n.velocity });
    }
    return result;
}

void ApplyMove(Layer& layer, const std::vector<NoteSnapshot>& originals, int64_t tickDelta, int keyDelta) {
    if (originals.empty()) return;
    int64_t minTick = originals.front().tick;
    int minKey = originals.front().key, maxKey = originals.front().key;
    for (const NoteSnapshot& o : originals) {
        minTick = std::min(minTick, o.tick);
        minKey = std::min(minKey, o.key);
        maxKey = std::max(maxKey, o.key);
    }
    tickDelta = std::max(tickDelta, -minTick);
    keyDelta = std::clamp(keyDelta, -minKey, 127 - maxKey);
    for (const NoteSnapshot& o : originals) {
        Note* n = FindNote(layer, o.uid);
        if (!n) continue;
        n->tick = o.tick + tickDelta;
        n->key = o.key + keyDelta;
    }
    SortNotes(layer);
}

void ApplyResize(Layer& layer, const std::vector<NoteSnapshot>& originals, int64_t lengthDelta, int64_t minLength) {
    minLength = std::max<int64_t>(1, minLength);
    for (const NoteSnapshot& o : originals) {
        Note* n = FindNote(layer, o.uid);
        if (!n) continue;
        n->length = std::max(minLength, o.length + lengthDelta);
    }
}

void ApplyVelocity(Layer& layer, const std::set<uint32_t>& uids, int velocity) {
    velocity = std::clamp(velocity, 1, 127);
    for (Note& n : layer.notes) if (uids.count(n.uid)) n.velocity = velocity;
}

std::vector<Note> CopyNotes(const Layer& layer, const std::set<uint32_t>& uids) {
    std::vector<Note> result;
    int64_t minTick = -1;
    for (const Note& n : layer.notes) {
        if (!uids.count(n.uid)) continue;
        if (minTick < 0 || n.tick < minTick) minTick = n.tick;
        result.push_back(n);
    }
    for (Note& n : result) {
        n.tick -= minTick;
        n.uid = 0;
    }
    return result;
}

int64_t SpanOf(const std::vector<Note>& notes) {
    int64_t end = 0;
    for (const Note& n : notes) end = std::max(end, n.tick + n.length);
    return end;
}

std::vector<uint32_t> PasteNotes(Sequence& sequence, int layerIndex, const std::vector<Note>& notes, int64_t atTick, uint32_t& next) {
    std::vector<uint32_t> ids;
    if (layerIndex < 0 || (size_t)layerIndex >= sequence.layers.size()) return ids;
    Layer& layer = sequence.layers[(size_t)layerIndex];
    for (Note n : notes) {
        n.tick += std::max<int64_t>(0, atTick);
        ClampNote(sequence, n);
        n.uid = next++;
        layer.notes.push_back(n);
        ids.push_back(n.uid);
    }
    SortNotes(layer);
    return ids;
}

std::string UniqueLayerName(const Sequence& sequence, const std::string& base, int ignoreIndex) {
    return UniqueName(LayerNames(sequence), base, ignoreIndex);
}

std::string UniqueInstrumentName(const Sequence& sequence, const std::string& base, int ignoreIndex) {
    return UniqueName(InstrumentNames(sequence), base, ignoreIndex);
}

int AddLayer(Sequence& sequence) {
    if ((int)sequence.layers.size() >= kMaxLayers) return -1;
    Layer layer;
    layer.name = UniqueLayerName(sequence, "Layer " + std::to_string(sequence.layers.size() + 1), -1);
    sequence.layers.push_back(layer);
    return (int)sequence.layers.size() - 1;
}

int DuplicateLayer(Sequence& sequence, int index, uint32_t& next) {
    if (index < 0 || (size_t)index >= sequence.layers.size()) return -1;
    if ((int)sequence.layers.size() >= kMaxLayers) return -1;
    Layer copy = sequence.layers[(size_t)index];
    copy.name = UniqueLayerName(sequence, copy.name + " Copy", -1);
    for (Note& n : copy.notes) n.uid = next++;
    sequence.layers.insert(sequence.layers.begin() + index + 1, copy);
    return index + 1;
}

bool RemoveLayer(Sequence& sequence, int index) {
    if (index < 0 || (size_t)index >= sequence.layers.size() || sequence.layers.size() <= 1) return false;
    sequence.layers.erase(sequence.layers.begin() + index);
    return true;
}

bool RenameLayer(Sequence& sequence, int index, const std::string& name) {
    if (index < 0 || (size_t)index >= sequence.layers.size()) return false;
    sequence.layers[(size_t)index].name = UniqueLayerName(sequence, name, index);
    return true;
}

int AddInstrument(Sequence& sequence) {
    if ((int)sequence.instruments.size() >= kMaxInstruments) return -1;
    Instrument inst;
    inst.name = UniqueInstrumentName(sequence, "Instrument " + std::to_string(sequence.instruments.size()), -1);
    inst.sets.push_back(SampleSet());
    sequence.instruments.push_back(inst);
    return (int)sequence.instruments.size() - 1;
}

bool RemoveInstrument(Sequence& sequence, int index) {
    if (index < 0 || (size_t)index >= sequence.instruments.size() || sequence.instruments.size() <= 1) return false;
    sequence.instruments.erase(sequence.instruments.begin() + index);
    for (Layer& layer : sequence.layers) {
        layer.notes.erase(std::remove_if(layer.notes.begin(), layer.notes.end(),
            [&](const Note& n) { return n.instrument == index; }), layer.notes.end());
        layer.controllers.erase(std::remove_if(layer.controllers.begin(), layer.controllers.end(),
            [&](const ControllerEvent& c) { return c.instrument == index; }), layer.controllers.end());
        for (Note& n : layer.notes) if (n.instrument > index) n.instrument--;
        for (ControllerEvent& c : layer.controllers) if (c.instrument > index) c.instrument--;
    }
    return true;
}

bool RenameInstrument(Sequence& sequence, int index, const std::string& name) {
    if (index < 0 || (size_t)index >= sequence.instruments.size()) return false;
    sequence.instruments[(size_t)index].name = UniqueInstrumentName(sequence, name, index);
    return true;
}

int AddSampleSet(Instrument& instrument) {
    SampleSet set;
    std::vector<std::string> names;
    for (const SampleSet& s : instrument.sets) names.push_back(s.name);
    set.name = UniqueName(names, "Set " + std::to_string(instrument.sets.size() + 1), -1);
    instrument.sets.push_back(set);
    return (int)instrument.sets.size() - 1;
}

bool RemoveSampleSet(Instrument& instrument, int index) {
    if (index < 0 || (size_t)index >= instrument.sets.size() || instrument.sets.size() <= 1) return false;
    instrument.sets.erase(instrument.sets.begin() + index);
    return true;
}

int AddZone(SampleSet& set, const std::string& assetPath) {
    SampleZone zone;
    zone.assetPath = assetPath;
    set.zones.push_back(zone);
    return (int)set.zones.size() - 1;
}

bool RemoveZone(SampleSet& set, int index) {
    if (index < 0 || (size_t)index >= set.zones.size()) return false;
    set.zones.erase(set.zones.begin() + index);
    return true;
}

bool SetBpm(Sequence& sequence, double bpm) {
    if (!(bpm >= 1.0 && bpm <= 999.0)) return false;
    if (sequence.tempo.empty() || sequence.tempo.front().tick != 0) sequence.tempo.insert(sequence.tempo.begin(), TempoPoint{ 0, bpm });
    else sequence.tempo.front().bpm = bpm;
    return true;
}

bool SetMeter(Sequence& sequence, int numerator, int denominator) {
    bool validDenominator = denominator == 1 || denominator == 2 || denominator == 4 || denominator == 8 || denominator == 16 || denominator == 32;
    if (!validDenominator || numerator < 1 || numerator > 32) return false;
    if (sequence.meter.empty() || sequence.meter.front().tick != 0) sequence.meter.insert(sequence.meter.begin(), MeterPoint{ 0, numerator, denominator });
    else {
        sequence.meter.front().numerator = numerator;
        sequence.meter.front().denominator = denominator;
    }
    return true;
}

int64_t BarsToTicks(const Sequence& sequence, double bars) {
    return (int64_t)std::llround(std::max(0.0, bars) * (double)sequence.BarTicksAt(0));
}

double TicksToBars(const Sequence& sequence, int64_t ticks) {
    return (double)ticks / (double)sequence.BarTicksAt(0);
}

}
