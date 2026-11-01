#pragma once
#include "Modules/Starsequence/SequenceData.h"
#include "Modules/Starsequence/SequenceAudioSource.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class FunctionUndoDocument;

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

    int SelectedLayer() const { return selectedLayer; }
    void SelectLayer(int index);

    bool IsLayerSolo(int layer) const;
    void SetLayerSolo(int layer, bool value);
    void SetLayerMuted(int layer, bool value);
    void SetLayerVolume(int layer, float value);
    void SetLayerPan(int layer, float value);

    bool loopEnabled = false;

    void Play();
    void Stop();
    void TogglePlay();
    bool IsPlaying() const;
    int ActiveVoices() const;
    double PlayheadTick() const;
    double CursorTick() const { return cursorTick; }
    void SetCursorTick(double tick);

    void Update();
    void SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive);

private:
    void EnsureSource();
    void ApplyLive(const std::string& path, float value);
    void CaptureUndoState(std::string& content, std::string& selection) const;
    void RestoreUndoState(const std::string& content, const std::string& selection);

    std::string filePath;
    Starsequence::Sequence sequence;
    bool isOpen = false;
    bool dirty = false;
    int structureRevision = 0;
    int selectedLayer = 0;
    double cursorTick = 0.0;

    std::unique_ptr<Starsequence::SequenceAudioSource> source;
    int sourceRevision = -1;
    std::vector<bool> solo;

    FunctionUndoDocument* undoDocument = nullptr;
    std::function<void()> undoReveal;
    std::function<bool()> undoOwnerActive;
};
