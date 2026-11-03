#include "SequenceDetailsPanel.h"
#include "SequenceEditorSession.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include "Editor/PropertyRowBuilders.h"
#include "Core/AudioBusRegistry.h"
#include <algorithm>
#include <cstdio>

using namespace Starsequence;

void SequenceDetailsPanel::SetSession(SequenceEditorSession* s) {
    session = s;
    buildRows = [this]() { return BuildRows(); };
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
    const DetailsContext context = s->GetDetailsContext();
    const std::string key = "C" + std::to_string((int)context) + "L" + std::to_string(layerIndex) + "/" + std::to_string(seq.layers.size()) +
        "I" + std::to_string(instrumentIndex) + "/" + std::to_string(seq.instruments.size()) +
        "S" + std::to_string(setIndex) + "/" + std::to_string(setCount) +
        "Z" + std::to_string(zoneCount) +
        "E" + std::to_string((int)s->SelectedEvent().kind) + "/" + std::to_string(s->SelectedEvent().index);

    if (context == DetailsContext::Sequence) {
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

    }

    if (context == DetailsContext::Layer && layerIndex >= 0 && (size_t)layerIndex < seq.layers.size()) {
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

    if (context == DetailsContext::Instrument && instrumentIndex >= 0 && (size_t)instrumentIndex < seq.instruments.size()) {
        const Instrument& inst = seq.instruments[(size_t)instrumentIndex];
        rows.push_back(HeaderRow("h.inst", "Instrument " + std::to_string(instrumentIndex)));
        rows.push_back(StringRow("inst.name", "Name", inst.name, [s, instrumentIndex](const std::string& v) {
            s->RenameInstrument(instrumentIndex, v);
        }));

        int zoneTotal = 0;
        for (const SampleSet& st : inst.sets) zoneTotal += (int)st.zones.size();
        rows.push_back(ReadOnlyRow("inst.sets", "Sample Sets", std::to_string(inst.sets.size())));
        rows.push_back(ReadOnlyRow("inst.zones", "Samples", std::to_string(zoneTotal)));
        rows.push_back(ButtonRow("inst.edit", "Edit Instrument...", [s]() { s->RequestInstrumentEditor(); }));
    }

    if (context == DetailsContext::Event && s->SelectedEvent().Valid()) {
        const EventRef ref = s->SelectedEvent();
        const bool anchor = IsAnchorEvent(seq, ref);
        const int64_t tick = EventTick(seq, ref);
        if (ref.kind == EventKind::Marker && (size_t)ref.index < seq.markers.size()) {
            rows.push_back(HeaderRow("h.event", "Marker"));
            rows.push_back(StringRow("ev.name", "Name", seq.markers[(size_t)ref.index].name, [s](const std::string& v) { s->RenameSelectedMarker(v); }));
        } else if (ref.kind == EventKind::Tempo && (size_t)ref.index < seq.tempo.size()) {
            rows.push_back(HeaderRow("h.event", "Tempo Change"));
            rows.push_back(FloatRow("ev.bpm", "BPM", (float)seq.tempo[(size_t)ref.index].bpm, false, [s, ref](float v) {
                Sequence& q = s->GetSequence();
                if ((size_t)ref.index < q.tempo.size() && v >= 1.0f && v <= 999.0f) {
                    q.tempo[(size_t)ref.index].bpm = (double)v;
                    s->EventsEdited();
                }
            }));
        } else if (ref.kind == EventKind::Meter && (size_t)ref.index < seq.meter.size()) {
            const MeterPoint& m = seq.meter[(size_t)ref.index];
            rows.push_back(HeaderRow("h.event", "Meter Change"));
            rows.push_back(StringRow("ev.meter", "Meter", std::to_string(m.numerator) + "/" + std::to_string(m.denominator), [s, ref](const std::string& v) {
                int n = 0, dn = 0;
                Sequence& q = s->GetSequence();
                if (std::sscanf(v.c_str(), "%d/%d", &n, &dn) != 2 || (size_t)ref.index >= q.meter.size()) return;
                bool validDenominator = dn == 1 || dn == 2 || dn == 4 || dn == 8 || dn == 16 || dn == 32;
                if (!validDenominator || n < 1 || n > 32) return;
                q.meter[(size_t)ref.index].numerator = n;
                q.meter[(size_t)ref.index].denominator = dn;
                s->EventsEdited();
            }));
        }
        if (!anchor) {
            rows.push_back(FloatRow("ev.tick", "Position (ticks)", (float)tick, true, [s](float v) { s->MoveSelectedEvent((int64_t)std::max(0.0f, v)); }));
            rows.push_back(ButtonRow("ev.delete", "Delete", [s]() { s->DeleteSelectedEvent(); }));
        } else {
            rows.push_back(ReadOnlyRow("ev.anchor", "Position", "Start of sequence"));
        }
    }

    for (PropertyRow& row : rows) row.id = key + "|" + row.id;
    return rows;
}
