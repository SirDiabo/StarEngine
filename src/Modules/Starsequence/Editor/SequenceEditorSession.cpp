#include "SequenceEditorSession.h"
#include "Modules/Starsequence/SequenceFormat.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include "Editor/EditorUndo.h"
#include "Core/UndoManager.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

using namespace Starsequence;

namespace {

std::vector<Note>& Clipboard() {
    static std::vector<Note> clipboard;
    return clipboard;
}

constexpr int kSnapDenominators[] = { 0, 1, 2, 4, 8, 16, 32 };
constexpr const char* kSnapLabels[] = { "Off", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
constexpr int kSnapCount = 7;
constexpr double kResyncDelaySeconds = 0.25;
constexpr size_t kMaxAuditions = 8;

}

SequenceEditorSession::SequenceEditorSession() {
    undoDocument = new FunctionUndoDocument();
    undoDocument->name = "Sequence Edit";
    undoDocument->capture = [this](std::string& content, std::string& selection) { CaptureUndoState(content, selection); };
    undoDocument->restore = [this](const std::string& content, const std::string& selection) { RestoreUndoState(content, selection); };
    undoDocument->reveal = [this]() { if (undoReveal) undoReveal(); };
    undoDocument->canTrack = [this]() {
        if (!isOpen) return false;
        return undoOwnerActive ? undoOwnerActive() : true;
    };
    UndoManager::Get().RegisterDocument(undoDocument);
}

SequenceEditorSession::~SequenceEditorSession() {
    Stop();
    auditions.clear();
    if (undoDocument) {
        UndoManager::Get().UnregisterDocument(undoDocument);
        delete undoDocument;
        undoDocument = nullptr;
    }
}

void SequenceEditorSession::SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive) {
    undoReveal = std::move(reveal);
    undoOwnerActive = std::move(isActive);
}

bool SequenceEditorSession::Open(const std::string& path) {
    filePath = path;
    ParseResult parsed;
    if (!LoadSequenceFile(path, parsed)) {
        for (const ParseDiagnostic& d : parsed.diagnostics) {
            if (d.error) TraceLog(LOG_ERROR, "Starsequence: %s line %d: %s", path.c_str(), d.line, d.message.c_str());
        }
        isOpen = false;
        return false;
    }
    for (const ParseDiagnostic& d : parsed.diagnostics) {
        if (!d.error) TraceLog(LOG_WARNING, "Starsequence: %s line %d: %s", path.c_str(), d.line, d.message.c_str());
    }
    sequence = parsed.sequence;
    sequence.Normalize();
    AssignNoteUids(sequence, nextUid);
    solo.assign(sequence.layers.size(), false);
    selectedLayer = 0;
    currentInstrument = 0;
    selectedSet = 0;
    selectedNotes.clear();
    cursorTick = 0.0;
    dirty = false;
    structureRevision++;
    isOpen = true;
    UndoManager::Get().ResetBaseline(undoDocument);
    return true;
}

bool SequenceEditorSession::Save() {
    if (!isOpen) return false;
    std::string error;
    if (!SaveSequenceFile(filePath, sequence, error)) {
        TraceLog(LOG_ERROR, "Starsequence: could not save %s: %s", filePath.c_str(), error.c_str());
        return false;
    }
    dirty = false;
    return true;
}

void SequenceEditorSession::MarkStructureChanged() {
    dirty = true;
    structureRevision++;
}

void SequenceEditorSession::GrowLengthToContent() {
    int64_t end = sequence.ComputeContentEnd();
    if (end <= sequence.length) return;
    int64_t bar = std::max<int64_t>(1, sequence.BarTicksAt(0));
    sequence.length = ((end + bar - 1) / bar) * bar;
}

void SequenceEditorSession::NotesEdited() {
    GrowLengthToContent();
    MarkStructureChanged();
}

void SequenceEditorSession::SelectLayer(int index) {
    int previous = selectedLayer;
    if (sequence.layers.empty()) { selectedLayer = 0; return; }
    selectedLayer = std::clamp(index, 0, (int)sequence.layers.size() - 1);
    if (selectedLayer != previous) selectedNotes.clear();
}

Layer* SequenceEditorSession::ActiveLayer() {
    if (selectedLayer < 0 || (size_t)selectedLayer >= sequence.layers.size()) return nullptr;
    return &sequence.layers[(size_t)selectedLayer];
}

const Layer* SequenceEditorSession::ActiveLayer() const {
    if (selectedLayer < 0 || (size_t)selectedLayer >= sequence.layers.size()) return nullptr;
    return &sequence.layers[(size_t)selectedLayer];
}

bool SequenceEditorSession::IsLayerSolo(int layer) const {
    return layer >= 0 && (size_t)layer < solo.size() && solo[(size_t)layer];
}

void SequenceEditorSession::ApplyLive(const std::string& path, float value) {
    if (!source) return;
    AudioParamValue v;
    v.number = value;
    source->SetParameter(path, v);
}

void SequenceEditorSession::SetLayerSolo(int layer, bool value) {
    if (layer < 0 || (size_t)layer >= sequence.layers.size()) return;
    if (solo.size() != sequence.layers.size()) solo.resize(sequence.layers.size(), false);
    solo[(size_t)layer] = value;
    ApplyLive("Layers/" + sequence.layers[(size_t)layer].name + "/Solo", value ? 1.0f : 0.0f);
}

void SequenceEditorSession::SetLayerMuted(int layer, bool value) {
    if (layer < 0 || (size_t)layer >= sequence.layers.size()) return;
    sequence.layers[(size_t)layer].muted = value;
    dirty = true;
    ApplyLive("Layers/" + sequence.layers[(size_t)layer].name + "/Mute", value ? 1.0f : 0.0f);
}

void SequenceEditorSession::SetLayerVolume(int layer, float value) {
    if (layer < 0 || (size_t)layer >= sequence.layers.size()) return;
    value = std::clamp(value, 0.0f, 2.0f);
    sequence.layers[(size_t)layer].volume = value;
    dirty = true;
    ApplyLive("Layers/" + sequence.layers[(size_t)layer].name + "/Volume", value);
}

void SequenceEditorSession::SetLayerPan(int layer, float value) {
    if (layer < 0 || (size_t)layer >= sequence.layers.size()) return;
    value = std::clamp(value, -1.0f, 1.0f);
    sequence.layers[(size_t)layer].pan = value;
    dirty = true;
    ApplyLive("Layers/" + sequence.layers[(size_t)layer].name + "/Pan", value);
}

int SequenceEditorSession::AddLayer() {
    int index = Starsequence::AddLayer(sequence);
    if (index < 0) return -1;
    solo.resize(sequence.layers.size(), false);
    SelectLayer(index);
    MarkStructureChanged();
    return index;
}

bool SequenceEditorSession::RemoveSelectedLayer() {
    int index = selectedLayer;
    if (!Starsequence::RemoveLayer(sequence, index)) return false;
    if ((size_t)index < solo.size()) solo.erase(solo.begin() + index);
    solo.resize(sequence.layers.size(), false);
    selectedNotes.clear();
    SelectLayer(std::min(index, (int)sequence.layers.size() - 1));
    MarkStructureChanged();
    return true;
}

void SequenceEditorSession::DuplicateSelectedLayer() {
    int index = Starsequence::DuplicateLayer(sequence, selectedLayer, nextUid);
    if (index < 0) return;
    solo.insert(solo.begin() + index, false);
    solo.resize(sequence.layers.size(), false);
    selectedNotes.clear();
    SelectLayer(index);
    MarkStructureChanged();
}

void SequenceEditorSession::RenameLayer(int index, const std::string& name) {
    if (!Starsequence::RenameLayer(sequence, index, name)) return;
    MarkStructureChanged();
}

void SequenceEditorSession::SetCurrentInstrument(int index) {
    if (sequence.instruments.empty()) { currentInstrument = 0; return; }
    int previous = currentInstrument;
    currentInstrument = std::clamp(index, 0, (int)sequence.instruments.size() - 1);
    if (currentInstrument != previous) selectedSet = 0;
}

int SequenceEditorSession::AddInstrument() {
    int index = Starsequence::AddInstrument(sequence);
    if (index < 0) return -1;
    SetCurrentInstrument(index);
    MarkStructureChanged();
    return index;
}

bool SequenceEditorSession::RemoveInstrument(int index) {
    if (!Starsequence::RemoveInstrument(sequence, index)) return false;
    selectedNotes.clear();
    selectedSet = 0;
    SetCurrentInstrument(std::min(currentInstrument, (int)sequence.instruments.size() - 1));
    MarkStructureChanged();
    return true;
}

void SequenceEditorSession::RenameInstrument(int index, const std::string& name) {
    if (!Starsequence::RenameInstrument(sequence, index, name)) return;
    MarkStructureChanged();
}

void SequenceEditorSession::SelectAllNotes() {
    selectedNotes.clear();
    if (const Layer* layer = ActiveLayer()) {
        for (const Note& n : layer->notes) selectedNotes.insert(n.uid);
    }
}

uint32_t SequenceEditorSession::AddNoteAt(int64_t tick, int key, int64_t length) {
    Note note;
    note.instrument = currentInstrument;
    note.tick = tick;
    note.key = key;
    note.length = length;
    note.velocity = defaultVelocity;
    uint32_t uid = Starsequence::AddNote(sequence, selectedLayer, note, nextUid);
    if (uid) NotesEdited();
    return uid;
}

bool SequenceEditorSession::DeleteSelectedNotes() {
    Layer* layer = ActiveLayer();
    if (!layer || selectedNotes.empty()) return false;
    int removed = Starsequence::DeleteNotes(*layer, selectedNotes);
    selectedNotes.clear();
    if (removed > 0) MarkStructureChanged();
    return removed > 0;
}

void SequenceEditorSession::CopySelection() {
    const Layer* layer = ActiveLayer();
    if (!layer || selectedNotes.empty()) return;
    Clipboard() = CopyNotes(*layer, selectedNotes);
}

bool SequenceEditorSession::CutSelection() {
    if (selectedNotes.empty()) return false;
    CopySelection();
    return DeleteSelectedNotes();
}

void SequenceEditorSession::Paste() {
    if (Clipboard().empty()) return;
    std::vector<uint32_t> ids = PasteNotes(sequence, selectedLayer, Clipboard(), SnapTick(cursorTick, false), nextUid);
    if (ids.empty()) return;
    selectedNotes = std::set<uint32_t>(ids.begin(), ids.end());
    NotesEdited();
}

void SequenceEditorSession::DuplicateSelection() {
    const Layer* layer = ActiveLayer();
    if (!layer || selectedNotes.empty()) return;
    std::vector<Note> copy = CopyNotes(*layer, selectedNotes);
    int64_t minTick = -1;
    for (uint32_t uid : selectedNotes) {
        const Note* n = FindNote(*layer, uid);
        if (n && (minTick < 0 || n->tick < minTick)) minTick = n->tick;
    }
    if (minTick < 0) return;
    int64_t span = std::max<int64_t>(SpanOf(copy), SnapTicks());
    std::vector<uint32_t> ids = PasteNotes(sequence, selectedLayer, copy, minTick + span, nextUid);
    if (ids.empty()) return;
    selectedNotes = std::set<uint32_t>(ids.begin(), ids.end());
    NotesEdited();
}

void SequenceEditorSession::CycleSnap(int direction) {
    snapIndex = ((snapIndex + direction) % kSnapCount + kSnapCount) % kSnapCount;
}

const char* SequenceEditorSession::SnapLabel() const {
    return kSnapLabels[snapIndex];
}

int64_t SequenceEditorSession::SnapTicks() const {
    int denominator = kSnapDenominators[snapIndex];
    if (denominator == 0) return 1;
    return std::max<int64_t>(1, (int64_t)sequence.resolution * 4 / denominator);
}

int64_t SequenceEditorSession::SnapTick(double tick, bool bypass) const {
    if (tick < 0.0) tick = 0.0;
    if (bypass || kSnapDenominators[snapIndex] == 0) return (int64_t)std::llround(tick);
    int64_t step = SnapTicks();
    return (int64_t)std::llround(tick / (double)step) * step;
}

void SequenceEditorSession::EnsureSource() {
    if (source && sourceRevision == structureRevision) return;
    if (source) source->Stop();
    source = std::make_unique<SequenceAudioSource>(sequence);
    sourceRevision = structureRevision;
    for (size_t l = 0; l < solo.size() && l < sequence.layers.size(); l++) {
        if (solo[l]) ApplyLive("Layers/" + sequence.layers[l].name + "/Solo", 1.0f);
    }
}

void SequenceEditorSession::PlayFrom(double tick) {
    if (!isOpen) return;
    EnsureSource();
    if (!source || !source->IsValid()) return;
    source->SetLooping(loopEnabled);
    source->Play();
    if (tick > 0.0) source->Seek(sequence.TickToSeconds(tick));
}

void SequenceEditorSession::Play() {
    PlayFrom(cursorTick);
}

void SequenceEditorSession::Stop() {
    if (source) source->Stop();
}

void SequenceEditorSession::TogglePlay() {
    if (IsPlaying()) Stop(); else Play();
}

bool SequenceEditorSession::IsPlaying() const {
    return source && source->IsPlaying();
}

int SequenceEditorSession::ActiveVoices() const {
    int voices = source && IsPlaying() ? source->ActiveVoices() : 0;
    for (const auto& a : auditions) voices += a->IsPlaying() ? a->ActiveVoices() : 0;
    return voices;
}

double SequenceEditorSession::PlayheadTick() const {
    if (source && IsPlaying()) return source->GetPositionTick();
    return cursorTick;
}

void SequenceEditorSession::SetCursorTick(double tick) {
    cursorTick = std::max(0.0, tick);
    if (IsPlaying()) source->Seek(sequence.TickToSeconds(cursorTick));
}

void SequenceEditorSession::AuditionNote(int instrument, int key, int velocity) {
    if (!isOpen || instrument < 0 || (size_t)instrument >= sequence.instruments.size()) return;
    Sequence a;
    a.name = "Audition";
    a.bus = sequence.bus;
    a.resolution = sequence.resolution;
    a.tempo.push_back({ 0, 120.0 });
    a.meter.push_back({ 0, 4, 4 });
    a.instruments = sequence.instruments;
    a.volume = sequence.volume;
    Layer layer;
    layer.name = "Audition";
    Note n;
    n.instrument = instrument;
    n.key = std::clamp(key, 0, 127);
    n.velocity = std::clamp(velocity, 1, 127);
    n.length = a.resolution / 2;
    layer.notes.push_back(n);
    a.layers.push_back(layer);
    a.length = (int64_t)a.resolution * 4;
    auto audition = std::make_unique<SequenceAudioSource>(a);
    if (!audition->IsValid()) return;
    audition->Play();
    if (auditions.size() >= kMaxAuditions) auditions.erase(auditions.begin());
    auditions.push_back(std::move(audition));
}

void SequenceEditorSession::Update() {
    if (source) source->SetLooping(loopEnabled);

    if (source && IsPlaying() && sourceRevision != structureRevision) {
        if (watchedRevision != structureRevision) {
            watchedRevision = structureRevision;
            watchedSince = GetTime();
        } else if (GetTime() - watchedSince >= kResyncDelaySeconds) {
            double tick = source->GetPositionTick();
            source->Stop();
            PlayFrom(tick);
        }
    }

    auditions.erase(std::remove_if(auditions.begin(), auditions.end(),
        [](const std::unique_ptr<SequenceAudioSource>& a) { return !a->IsPlaying(); }), auditions.end());
}

void SequenceEditorSession::CaptureUndoState(std::string& content, std::string& selection) const {
    content = WriteSequence(sequence);
    selection = std::to_string(selectedLayer) + "," + std::to_string(currentInstrument);
}

void SequenceEditorSession::RestoreUndoState(const std::string& content, const std::string& selection) {
    ParseResult parsed = ParseSequence(content);
    if (parsed.HasErrors()) return;
    sequence = parsed.sequence;
    sequence.Normalize();
    AssignNoteUids(sequence, nextUid);
    solo.resize(sequence.layers.size(), false);
    selectedNotes.clear();
    int layerIndex = 0, instrumentIndex = 0;
    size_t comma = selection.find(',');
    try {
        layerIndex = std::stoi(selection.substr(0, comma));
        if (comma != std::string::npos) instrumentIndex = std::stoi(selection.substr(comma + 1));
    } catch (...) {}
    SelectLayer(layerIndex);
    SetCurrentInstrument(instrumentIndex);
    dirty = true;
    structureRevision++;
}
