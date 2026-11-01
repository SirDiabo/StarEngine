#include "SequenceEditorSession.h"
#include "Modules/Starsequence/SequenceFormat.h"
#include "Editor/EditorUndo.h"
#include "Core/UndoManager.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

using namespace Starsequence;

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
    solo.assign(sequence.layers.size(), false);
    selectedLayer = 0;
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

void SequenceEditorSession::SelectLayer(int index) {
    if (sequence.layers.empty()) { selectedLayer = 0; return; }
    selectedLayer = std::clamp(index, 0, (int)sequence.layers.size() - 1);
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

void SequenceEditorSession::EnsureSource() {
    if (source && sourceRevision == structureRevision) return;
    if (source) source->Stop();
    source = std::make_unique<SequenceAudioSource>(sequence);
    sourceRevision = structureRevision;
    for (size_t l = 0; l < solo.size() && l < sequence.layers.size(); l++) {
        if (solo[l]) ApplyLive("Layers/" + sequence.layers[l].name + "/Solo", 1.0f);
    }
}

void SequenceEditorSession::Play() {
    if (!isOpen) return;
    EnsureSource();
    if (!source || !source->IsValid()) return;
    source->SetLooping(loopEnabled);
    source->Play();
    if (cursorTick > 0.0) source->Seek(sequence.TickToSeconds(cursorTick));
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
    return source && IsPlaying() ? source->ActiveVoices() : 0;
}

double SequenceEditorSession::PlayheadTick() const {
    if (source && IsPlaying()) return source->GetPositionTick();
    return cursorTick;
}

void SequenceEditorSession::SetCursorTick(double tick) {
    cursorTick = std::max(0.0, tick);
    if (IsPlaying()) source->Seek(sequence.TickToSeconds(cursorTick));
}

void SequenceEditorSession::Update() {
    if (source) source->SetLooping(loopEnabled);
}

void SequenceEditorSession::CaptureUndoState(std::string& content, std::string& selection) const {
    content = WriteSequence(sequence);
    selection = std::to_string(selectedLayer);
}

void SequenceEditorSession::RestoreUndoState(const std::string& content, const std::string& selection) {
    ParseResult parsed = ParseSequence(content);
    if (parsed.HasErrors()) return;
    sequence = parsed.sequence;
    sequence.Normalize();
    solo.resize(sequence.layers.size(), false);
    int index = 0;
    try { index = std::stoi(selection); } catch (...) { index = 0; }
    SelectLayer(index);
    dirty = true;
    structureRevision++;
}
