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
    enum class DragMode { None, Create, Move, Resize, Box, SelectOrPlace, Brush, Velocity };

    struct Layout {
        Rectangle ruler{};
        Rectangle keyboard{};
        Rectangle grid{};
        Rectangle strip{};
        Rectangle lane{};
        bool hasLane = false;
        bool hasStrip = false;
    };

    Layout ComputeLayout() const;
    void FitInitialView(float viewHeight);
    void HandleNavigation(const Layout& layout, bool hover);
    void HandleGridInput(const Layout& layout, bool hover);
    void HandleLaneInput(const Layout& layout, bool hover);
    void HandleKeyboardColumn(const Layout& layout, bool hover);
    void HandleShortcuts(bool hover);
    void FinishDrag();
    void HandleStripInput(const Layout& layout, bool hover);
    bool BeginRulerDrag(const Layout& layout);
    void UpdateRulerDrag(const Layout& layout);
    Starsequence::EventRef HitEvent(const Layout& layout, Starsequence::EventKind kind, Vector2 mouse) const;
    std::string EventLabel(Starsequence::EventKind kind, int index) const;
    void DrawStrip(const Layout& layout);
    void StampBrush(const Rectangle& grid, double mouseTick, int mouseKey, int64_t step, bool snapOn);
    void EraseAlong(const Rectangle& grid, Vector2 from, Vector2 to);
    void BeginNoteDrag(uint32_t uid, double mouseTick, int mouseKey, DragMode mode);
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

    enum class RulerDrag { None, LoopStart, LoopEnd, LoopMove, Length };
    RulerDrag rulerDrag = RulerDrag::None;
    int64_t rulerDragOffset = 0;
    bool stripDragging = false;
    int64_t stripLastTick = -1;
    double lastClickTime = -10.0;
    Vector2 lastClickPos = { 0, 0 };

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
    bool createMoved = false;
    bool boxMoved = false;
    int64_t brushFirstTick = 0;
    int64_t brushSpacing = 120;
    int64_t brushLength = 120;
    int64_t brushLastIndex = 0;
    int brushKey = 60;
    int brushLastKey = 60;
    bool erasing = false;
    Vector2 eraseLast = { 0, 0 };
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
