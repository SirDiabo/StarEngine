#pragma once
#include "SequenceData.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace Starsequence {

struct NoteSnapshot {
    uint32_t uid = 0;
    int64_t tick = 0;
    int64_t length = 0;
    int key = 60;
    int velocity = 100;
};

void SortNotes(Layer& layer);
void AssignNoteUids(Sequence& sequence, uint32_t& next);
Note* FindNote(Layer& layer, uint32_t uid);
const Note* FindNote(const Layer& layer, uint32_t uid);

uint32_t AddNote(Sequence& sequence, int layerIndex, Note note, uint32_t& next);
int DeleteNotes(Layer& layer, const std::set<uint32_t>& uids);
std::vector<NoteSnapshot> SnapshotNotes(const Layer& layer, const std::set<uint32_t>& uids);
void ApplyMove(Layer& layer, const std::vector<NoteSnapshot>& originals, int64_t tickDelta, int keyDelta);
void ApplyResize(Layer& layer, const std::vector<NoteSnapshot>& originals, int64_t lengthDelta, int64_t minLength);
void ApplyVelocity(Layer& layer, const std::set<uint32_t>& uids, int velocity);

std::vector<Note> CopyNotes(const Layer& layer, const std::set<uint32_t>& uids);
std::vector<uint32_t> PasteNotes(Sequence& sequence, int layerIndex, const std::vector<Note>& notes, int64_t atTick, uint32_t& next);
int64_t SpanOf(const std::vector<Note>& notes);

std::string UniqueLayerName(const Sequence& sequence, const std::string& base, int ignoreIndex);
std::string UniqueInstrumentName(const Sequence& sequence, const std::string& base, int ignoreIndex);
int AddLayer(Sequence& sequence);
int DuplicateLayer(Sequence& sequence, int index, uint32_t& next);
bool RemoveLayer(Sequence& sequence, int index);
bool RenameLayer(Sequence& sequence, int index, const std::string& name);

int AddInstrument(Sequence& sequence);
bool RemoveInstrument(Sequence& sequence, int index);
bool RenameInstrument(Sequence& sequence, int index, const std::string& name);
int AddSampleSet(Instrument& instrument);
bool RemoveSampleSet(Instrument& instrument, int index);
int AddZone(SampleSet& set, const std::string& assetPath);
bool RemoveZone(SampleSet& set, int index);

bool SetBpm(Sequence& sequence, double bpm);
bool SetMeter(Sequence& sequence, int numerator, int denominator);
enum class EventKind { Marker, Tempo, Meter };

struct EventRef {
    EventKind kind = EventKind::Marker;
    int index = -1;
    bool Valid() const { return index >= 0; }
};

int64_t BarStartTick(const Sequence& sequence, int64_t tick);
int64_t EventTick(const Sequence& sequence, EventRef ref);
bool IsAnchorEvent(const Sequence& sequence, EventRef ref);
std::string UniqueMarkerName(const Sequence& sequence, const std::string& base);
EventRef AddMarker(Sequence& sequence, int64_t tick, const std::string& name);
EventRef AddTempoPoint(Sequence& sequence, int64_t tick, double bpm);
EventRef AddMeterPoint(Sequence& sequence, int64_t tick, int numerator, int denominator);
bool RemoveEvent(Sequence& sequence, EventRef ref);
EventRef MoveEvent(Sequence& sequence, EventRef ref, int64_t newTick);

int64_t BarsToTicks(const Sequence& sequence, double bars);
double TicksToBars(const Sequence& sequence, int64_t ticks);

}
