#include "SequenceDetailsPanel.h"
#include "SequenceEditorSession.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include "Editor/PropertyRowBuilders.h"
#include "Core/AudioBusRegistry.h"
#include <algorithm>
#include <cstdio>

using namespace Starsequence;

namespace {

const char* LoopName(LoopMode mode) {
    switch (mode) {
        case LoopMode::Whole: return "Whole";
        case LoopMode::Region: return "Region";
        case LoopMode::Wav: return "Wav";
        default: return "None";
    }
}

LoopMode LoopFromName(const std::string& name) {
    if (name == "Whole") return LoopMode::Whole;
    if (name == "Region") return LoopMode::Region;
    if (name == "Wav") return LoopMode::Wav;
    return LoopMode::None;
}

SampleSet* FindSet(SequenceEditorSession* s, int instrument, int set) {
    Sequence& seq = s->GetSequence();
    if (instrument < 0 || (size_t)instrument >= seq.instruments.size()) return nullptr;
    Instrument& inst = seq.instruments[(size_t)instrument];
    if (set < 0 || (size_t)set >= inst.sets.size()) return nullptr;
    return &inst.sets[(size_t)set];
}

SampleZone* FindZone(SequenceEditorSession* s, int instrument, int set, int zone) {
    SampleSet* sampleSet = FindSet(s, instrument, set);
    if (!sampleSet || zone < 0 || (size_t)zone >= sampleSet->zones.size()) return nullptr;
    return &sampleSet->zones[(size_t)zone];
}

}

void SequenceDetailsPanel::SetSession(SequenceEditorSession* s) {
    session = s;
    buildRows = [this]() { return BuildRows(); };
    onAssetPicked = [this](const std::string& rowId, const std::string& path) { OnAssetPickedForRow(rowId, path); };
}

void SequenceDetailsPanel::OnAssetPickedForRow(const std::string& fullRowId, const std::string& pickedPath) {
    if (!session) return;
    size_t separator = fullRowId.rfind('|');
    std::string rowId = separator == std::string::npos ? fullRowId : fullRowId.substr(separator + 1);
    int zoneIndex = -1;
    if (std::sscanf(rowId.c_str(), "z%d.sample", &zoneIndex) != 1) return;
    SampleZone* zone = FindZone(session, session->CurrentInstrument(), session->SelectedSet(), zoneIndex);
    if (!zone) return;
    zone->assetPath = pickedPath;
    session->MarkStructureChanged();
}

std::vector<PropertyRow> SequenceDetailsPanel::BuildRows() {
    std::vector<PropertyRow> rows;
    if (!session || !session->IsOpen()) return rows;
    using namespace PropertyRows;

    SequenceEditorSession* s = session;
    Sequence& seq = s->GetSequence();
    const int layerIndex = s->SelectedLayer();
    const int instrumentIndex = s->CurrentInstrument();
    const int setIndex = std::clamp(s->SelectedSet(), 0, 1000);

    int setCount = 0, zoneCount = 0;
    if (instrumentIndex >= 0 && (size_t)instrumentIndex < seq.instruments.size()) {
        const Instrument& inst = seq.instruments[(size_t)instrumentIndex];
        setCount = (int)inst.sets.size();
        if ((size_t)setIndex < inst.sets.size()) zoneCount = (int)inst.sets[(size_t)setIndex].zones.size();
    }
    const std::string key = "L" + std::to_string(layerIndex) + "/" + std::to_string(seq.layers.size()) +
        "I" + std::to_string(instrumentIndex) + "/" + std::to_string(seq.instruments.size()) +
        "S" + std::to_string(setIndex) + "/" + std::to_string(setCount) +
        "Z" + std::to_string(zoneCount);

    rows.push_back(HeaderRow("h.seq", "Sequence"));
    rows.push_back(StringRow("seq.name", "Name", seq.name, [s](const std::string& v) {
        s->GetSequence().name = v;
        s->MarkStructureChanged();
    }));
    rows.push_back(EnumRow("seq.bus", "Bus", seq.bus, AudioBusRegistry::Get().Names(), [s](const std::string& v) {
        s->GetSequence().bus = v;
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.bpm", "BPM", (float)(seq.tempo.empty() ? 120.0 : seq.tempo.front().bpm), false, [s](float v) {
        if (SetBpm(s->GetSequence(), (double)v)) s->MarkStructureChanged();
    }));
    {
        const MeterPoint m = seq.meter.empty() ? MeterPoint{ 0, 4, 4 } : seq.meter.front();
        rows.push_back(StringRow("seq.meter", "Meter", std::to_string(m.numerator) + "/" + std::to_string(m.denominator), [s](const std::string& v) {
            int n = 0, d = 0;
            if (std::sscanf(v.c_str(), "%d/%d", &n, &d) == 2 && SetMeter(s->GetSequence(), n, d)) s->MarkStructureChanged();
        }));
    }
    rows.push_back(FloatRow("seq.length", "Length (bars)", (float)TicksToBars(seq, seq.length), false, [s](float v) {
        Sequence& q = s->GetSequence();
        int64_t ticks = std::max<int64_t>(BarsToTicks(q, v), q.BarTicksAt(0));
        q.length = ticks;
        if (q.loopEnd > q.length) q.loopEnd = q.length;
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.loopStart", "Loop Start (bars)", (float)TicksToBars(seq, seq.loopStart), false, [s](float v) {
        Sequence& q = s->GetSequence();
        q.loopStart = std::min(BarsToTicks(q, v), q.length);
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.loopEnd", "Loop End (bars)", (float)TicksToBars(seq, seq.loopEnd), false, [s](float v) {
        Sequence& q = s->GetSequence();
        q.loopEnd = std::min(BarsToTicks(q, v), q.length);
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.poly", "Polyphony", (float)seq.polyphony, true, [s](float v) {
        s->GetSequence().polyphony = std::clamp((int)v, 1, kMaxPolyphony);
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.fade", "Layer Fade", seq.layerFade, false, [s](float v) {
        s->GetSequence().layerFade = std::clamp(v, 0.0f, 30.0f);
        s->MarkStructureChanged();
    }));
    rows.push_back(FloatRow("seq.volume", "Volume", seq.volume, false, [s](float v) {
        s->GetSequence().volume = std::clamp(v, 0.0f, 4.0f);
        s->MarkStructureChanged();
    }));
    rows.push_back(ReadOnlyRow("seq.tempoCount", "Tempo Points", std::to_string(seq.tempo.size())));
    rows.push_back(ReadOnlyRow("seq.resolution", "Resolution", std::to_string(seq.resolution) + " ticks/beat"));

    if (layerIndex >= 0 && (size_t)layerIndex < seq.layers.size()) {
        const Layer& layer = seq.layers[(size_t)layerIndex];
        rows.push_back(HeaderRow("h.layer", "Layer"));
        rows.push_back(StringRow("layer.name", "Name", layer.name, [s, layerIndex](const std::string& v) {
            s->RenameLayer(layerIndex, v);
        }));
        rows.push_back(FloatRow("layer.volume", "Volume", layer.volume, false, [s, layerIndex](float v) {
            s->SetLayerVolume(layerIndex, v);
        }));
        rows.push_back(FloatRow("layer.pan", "Pan", layer.pan, false, [s, layerIndex](float v) {
            s->SetLayerPan(layerIndex, v);
        }));
        rows.push_back(BoolRow("layer.muted", "Muted", layer.muted, [s, layerIndex](bool v) {
            s->SetLayerMuted(layerIndex, v);
        }));
        rows.push_back(ReadOnlyRow("layer.notes", "Notes", std::to_string(layer.notes.size())));
    }

    if (instrumentIndex >= 0 && (size_t)instrumentIndex < seq.instruments.size()) {
        const Instrument& inst = seq.instruments[(size_t)instrumentIndex];
        rows.push_back(HeaderRow("h.inst", "Instrument " + std::to_string(instrumentIndex)));
        rows.push_back(StringRow("inst.name", "Name", inst.name, [s, instrumentIndex](const std::string& v) {
            s->RenameInstrument(instrumentIndex, v);
        }));

        if ((size_t)setIndex >= inst.sets.size()) {
            s->SetSelectedSet(0);
            for (PropertyRow& row : rows) row.id = key + "|" + row.id;
            return rows;
        }
        const SampleSet& set = inst.sets[(size_t)setIndex];

        std::vector<std::string> setNames;
        for (const SampleSet& st : inst.sets) setNames.push_back(st.name);
        rows.push_back(EnumRow("inst.set", "Sample Set", set.name, setNames, [s, instrumentIndex](const std::string& v) {
            Sequence& q = s->GetSequence();
            if (instrumentIndex < 0 || (size_t)instrumentIndex >= q.instruments.size()) return;
            const Instrument& i = q.instruments[(size_t)instrumentIndex];
            for (size_t n = 0; n < i.sets.size(); n++) {
                if (i.sets[n].name == v) { s->SetSelectedSet((int)n); return; }
            }
        }));
        rows.push_back(ButtonRow("inst.addSet", "Add Sample Set", [s, instrumentIndex]() {
            Sequence& q = s->GetSequence();
            if (instrumentIndex < 0 || (size_t)instrumentIndex >= q.instruments.size()) return;
            int added = AddSampleSet(q.instruments[(size_t)instrumentIndex]);
            s->SetSelectedSet(added);
            s->MarkStructureChanged();
        }));
        if (inst.sets.size() > 1) {
            rows.push_back(ButtonRow("inst.removeSet", "Remove Sample Set", [s, instrumentIndex, setIndex]() {
                Sequence& q = s->GetSequence();
                if (instrumentIndex < 0 || (size_t)instrumentIndex >= q.instruments.size()) return;
                if (RemoveSampleSet(q.instruments[(size_t)instrumentIndex], setIndex)) {
                    s->SetSelectedSet(0);
                    s->MarkStructureChanged();
                }
            }));
        }
        rows.push_back(StringRow("set.name", "Set Name", set.name, [s, instrumentIndex, setIndex](const std::string& v) {
            SampleSet* st = FindSet(s, instrumentIndex, setIndex);
            if (!st || v.empty()) return;
            Instrument& i = s->GetSequence().instruments[(size_t)instrumentIndex];
            for (size_t n = 0; n < i.sets.size(); n++) {
                if ((int)n != setIndex && i.sets[n].name == v) return;
            }
            st->name = v;
            s->MarkStructureChanged();
        }));
        rows.push_back(FloatRow("set.volume", "Volume", set.volume, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->volume = std::clamp(v, 0.0f, 4.0f); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.pan", "Pan", set.pan, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->pan = std::clamp(v, -1.0f, 1.0f); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.bend", "Bend Range", (float)set.bendRange, true, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->bendRange = std::clamp((int)v, 0, 48); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.attack", "Attack (s)", set.attack, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->attack = std::max(0.0f, v); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.decay", "Decay (s)", set.decay, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->decay = std::max(0.0f, v); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.sustain", "Sustain", set.sustain, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->sustain = std::clamp(v, 0.0f, 1.0f); s->MarkStructureChanged(); }
        }));
        rows.push_back(FloatRow("set.release", "Release (s)", set.release, false, [s, instrumentIndex, setIndex](float v) {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) { st->release = std::max(0.0f, v); s->MarkStructureChanged(); }
        }));

        rows.push_back(HeaderRow("h.samples", "Samples"));
        for (size_t z = 0; z < set.zones.size(); z++) {
            const SampleZone& zone = set.zones[z];
            const int zi = (int)z;
            const std::string p = "z" + std::to_string(zi) + ".";
            rows.push_back(HeaderRow(p + "h", "Sample " + std::to_string(zi + 1)));
            rows.push_back(AssetRow(p + "sample", "Sound", ".starsound", StemOrNone(zone.assetPath)));
            auto zoneInt = [s, instrumentIndex, setIndex, zi](int SampleZone::*field, int lo, int hi) {
                return [=](float v) {
                    if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->*field = std::clamp((int)v, lo, hi); s->MarkStructureChanged(); }
                };
            };
            auto zoneFloat = [s, instrumentIndex, setIndex, zi](float SampleZone::*field, float lo, float hi) {
                return [=](float v) {
                    if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->*field = std::clamp(v, lo, hi); s->MarkStructureChanged(); }
                };
            };
            rows.push_back(FloatRow(p + "keyLow", "Key Low", (float)zone.keyLow, true, zoneInt(&SampleZone::keyLow, 0, 127)));
            rows.push_back(FloatRow(p + "keyHigh", "Key High", (float)zone.keyHigh, true, zoneInt(&SampleZone::keyHigh, 0, 127)));
            rows.push_back(FloatRow(p + "root", "Root Key", (float)zone.rootKey, true, zoneInt(&SampleZone::rootKey, 0, 127)));
            rows.push_back(FloatRow(p + "velLow", "Velocity Low", (float)zone.velLow, true, zoneInt(&SampleZone::velLow, 1, 127)));
            rows.push_back(FloatRow(p + "velHigh", "Velocity High", (float)zone.velHigh, true, zoneInt(&SampleZone::velHigh, 1, 127)));
            rows.push_back(BoolRow(p + "pitched", "Pitched", zone.pitched, [s, instrumentIndex, setIndex, zi](bool v) {
                if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->pitched = v; s->MarkStructureChanged(); }
            }));
            rows.push_back(FloatRow(p + "tune", "Tune (cents)", zone.tuneCents, false, zoneFloat(&SampleZone::tuneCents, -2400.0f, 2400.0f)));
            rows.push_back(FloatRow(p + "gain", "Gain (dB)", zone.gainDb, false, zoneFloat(&SampleZone::gainDb, -48.0f, 24.0f)));
            rows.push_back(FloatRow(p + "pan", "Pan", zone.pan, false, zoneFloat(&SampleZone::pan, -1.0f, 1.0f)));
            rows.push_back(EnumRow(p + "loop", "Loop", LoopName(zone.loop), { "None", "Whole", "Region", "Wav" },
                [s, instrumentIndex, setIndex, zi](const std::string& v) {
                    if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->loop = LoopFromName(v); s->MarkStructureChanged(); }
                }));
            if (zone.loop == LoopMode::Region) {
                rows.push_back(FloatRow(p + "loopStart", "Loop Start", (float)zone.loopStart, true, [s, instrumentIndex, setIndex, zi](float v) {
                    if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->loopStart = (uint32_t)std::max(0.0f, v); s->MarkStructureChanged(); }
                }));
                rows.push_back(FloatRow(p + "loopEnd", "Loop End", (float)zone.loopEnd, true, [s, instrumentIndex, setIndex, zi](float v) {
                    if (SampleZone* zn = FindZone(s, instrumentIndex, setIndex, zi)) { zn->loopEnd = (uint32_t)std::max(0.0f, v); s->MarkStructureChanged(); }
                }));
            }
            rows.push_back(ButtonRow(p + "remove", "Remove Sample", [s, instrumentIndex, setIndex, zi]() {
                if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) {
                    if (RemoveZone(*st, zi)) s->MarkStructureChanged();
                }
            }));
        }
        rows.push_back(ButtonRow("zone.add", "Add Sample", [s, instrumentIndex, setIndex]() {
            if (SampleSet* st = FindSet(s, instrumentIndex, setIndex)) {
                AddZone(*st, "");
                s->MarkStructureChanged();
            }
        }));
    }

    for (PropertyRow& row : rows) row.id = key + "|" + row.id;
    return rows;
}
