#pragma once
#include "Core/Panel.h"
#include "PianoRollView.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include "raylib.h"
#include <cstdint>
#include <set>
#include <vector>

class SequenceEditorSession;

class PianoRollPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }

private:
    enum class DragMode { None, Create, Move, Resize, Box, Velocity };

    struct Layout {
        Rectangle ruler{};
        Rectangle keyboard{};
        Rectangle grid{};
        Rectangle lane{};
        bool hasLane = false;
    };

    Layout ComputeLayout() const;
    void FitInitialView(float viewHeight);
    void HandleNavigation(const Layout& layout, bool hover);
    void HandleGridInput(const Layout& layout, bool hover);
    void HandleLaneInput(const Layout& layout, bool hover);
    void HandleKeyboardColumn(const Layout& layout, bool hover);
    void HandleShortcuts(bool hover);
    uint32_t HitNote(const Rectangle& grid, Vector2 mouse, bool& onEdge) const;
    void DrawNotes(const Rectangle& grid, double tickStart, double tickEnd);
    void DrawVelocityLane(const Layout& layout, double tickStart, double tickEnd);

    SequenceEditorSession* session = nullptr;
    Starsequence::PianoRollView view;
    std::vector<Starsequence::GridLine> gridLines;
    bool viewFitted = false;
    bool scrubbing = false;
    bool panning = false;
    Vector2 lastMouse = { 0, 0 };

    DragMode drag = DragMode::None;
    std::vector<Starsequence::NoteSnapshot> dragSnapshots;
    uint32_t dragNote = 0;
    int64_t dragRefTick = 0;
    int64_t dragRefLength = 0;
    int dragRefKey = 60;
    int dragInstrument = 0;
    double dragStartTick = 0.0;
    int dragStartKey = 60;
    int64_t lastTickDelta = 0;
    int lastKeyDelta = 0;
    int64_t lastLengthDelta = 0;
    Vector2 boxStart = { 0, 0 };
    std::set<uint32_t> boxBase;
    std::set<uint32_t> velocityTargets;
};

class SequenceLayersPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }

private:
    SequenceEditorSession* session = nullptr;
    float scroll = 0.0f;
    int dragLayer = -1;
    int dragKind = 0;
};

class SequenceInstrumentsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }

private:
    SequenceEditorSession* session = nullptr;
    float scroll = 0.0f;
};
