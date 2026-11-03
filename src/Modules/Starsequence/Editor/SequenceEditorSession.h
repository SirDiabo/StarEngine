#pragma once
#include "Modules/Starsequence/SequenceData.h"
#include "Modules/Starsequence/SequenceAudioSource.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

class FunctionUndoDocument;

enum class EditTool { Select, Pencil, Brush };
enum class DetailsContext { Sequence, Layer, Instrument, Event };

class SequenceEditorSession {
public:
    SequenceEditorSession();
    ~SequenceEditorSession();

    bool Open(const std::string& path);
    bool Save();
    bool IsOpen() const { return isOpen; }
    bool IsDirty() const { return dirty; }
    const std::string& GetPath() const { return filePath; }

    Starsequence::Sequence& GetSequence() { return sequence; }
    const Starsequence::Sequence& GetSequence() const { return sequence; }
    int StructureRevision() const { return structureRevision; }
    void MarkDirty() { dirty = true; }
    void MarkStructureChanged();
    void NotesEdited();

    int SelectedLayer() const { return selectedLayer; }
    void SelectLayer(int index);
    Starsequence::Layer* ActiveLayer();
    const Starsequence::Layer* ActiveLayer() const;

    bool IsLayerSolo(int layer) const;
    void SetLayerSolo(int layer, bool value);
    void SetLayerMuted(int layer, bool value);
    void SetLayerVolume(int layer, float value);
    void SetLayerPan(int layer, float value);
    int AddLayer();
    bool RemoveSelectedLayer();
    void DuplicateSelectedLayer();
    void RenameLayer(int index, const std::string& name);

    int CurrentInstrument() const { return currentInstrument; }
    void SetCurrentInstrument(int index);
    int SelectedSet() const { return selectedSet; }
    void SetSelectedSet(int index) { selectedSet = index < 0 ? 0 : index; }
    int AddInstrument();
    int ImportSampleSet(int instrument, Starsequence::SampleSet set);
    int AddInstrumentFromSet(const std::string& name, Starsequence::SampleSet set);
    bool RemoveInstrument(int index);
    void RenameInstrument(int index, const std::string& name);

    const std::set<uint32_t>& SelectedNotes() const { return selectedNotes; }
    void SetSelectedNotes(const std::set<uint32_t>& notes) { selectedNotes = notes; }
    void ClearNoteSelection() { selectedNotes.clear(); }
    void SelectAllNotes();
    uint32_t AddNoteAt(int64_t tick, int key, int64_t length);
    bool DeleteSelectedNotes();
    void CopySelection();
    bool CutSelection();
    void Paste();
    void DuplicateSelection();

    int SnapIndex() const { return snapIndex; }
    void CycleSnap(int direction);
    const char* SnapLabel() const;
    int64_t SnapTicks() const;
    int64_t SnapTick(double tick, bool bypass) const;
    int defaultVelocity = 100;
    bool typingInPopup = false;
    std::function<void()> onOpenInstrumentEditor;
    void RequestInstrumentEditor() { if (onOpenInstrumentEditor) onOpenInstrumentEditor(); }

    EditTool tool = EditTool::Brush;
    DetailsContext GetDetailsContext() const { return detailsContext; }
    void SetDetailsContext(DetailsContext context) { detailsContext = context; }
    Starsequence::EventRef SelectedEvent() const { return selectedEvent; }
    void SelectEvent(Starsequence::EventRef ref);
    void ClearEventSelection() { selectedEvent = {}; }
    Starsequence::EventRef AddEventAt(Starsequence::EventKind kind, int64_t tick);
    bool DeleteSelectedEvent();
    void MoveSelectedEvent(int64_t tick);
    void RenameSelectedMarker(const std::string& name);
    void EventsEdited() { MarkStructureChanged(); }
    int64_t NoteLength() const;
    void SetNoteLength(int64_t length);
    bool NoteExistsAt(int64_t tick, int key) const;
    int EraseNotes(const std::set<uint32_t>& uids);

    bool loopEnabled = false;

    void Play();
    void Stop();
    void TogglePlay();
    bool IsPlaying() const;
    int ActiveVoices() const;
    double PlayheadTick() const;
    double CursorTick() const { return cursorTick; }
    void SetCursorTick(double tick);
    void AuditionNote(int instrument, int key, int velocity, int setIndex = -1);
    void BeginHeldAudition(int instrument, int key, int velocity, int setIndex = -1);
    void EndHeldAudition();

    void Update();
    void SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive);

private:
    void EnsureSource();
    void PlayFrom(double tick);
    void ApplyLive(const std::string& path, float value);
    void GrowLengthToContent();
    void CaptureUndoState(std::string& content, std::string& selection) const;
    void RestoreUndoState(const std::string& content, const std::string& selection);

    std::string filePath;
    Starsequence::Sequence sequence;
    bool isOpen = false;
    bool dirty = false;
    int structureRevision = 0;
    int selectedLayer = 0;
    int currentInstrument = 0;
    int selectedSet = 0;
    int snapIndex = 5;
    int64_t noteLength = 0;
    Starsequence::EventRef selectedEvent;
    DetailsContext detailsContext = DetailsContext::Sequence;
    double cursorTick = 0.0;
    uint32_t nextUid = 1;
    std::set<uint32_t> selectedNotes;

    std::unique_ptr<Starsequence::SequenceAudioSource> source;
    std::vector<std::unique_ptr<Starsequence::SequenceAudioSource>> auditions;
    std::unique_ptr<Starsequence::SequenceAudioSource> heldAudition;
    std::unique_ptr<Starsequence::SequenceAudioSource> BuildAudition(int instrument, int key, int velocity, int setIndex, int64_t lengthTicks);
    int sourceRevision = -1;
    int watchedRevision = -1;
    double watchedSince = 0.0;
    std::vector<bool> solo;

    FunctionUndoDocument* undoDocument = nullptr;
    std::function<void()> undoReveal;
    std::function<bool()> undoOwnerActive;
};
