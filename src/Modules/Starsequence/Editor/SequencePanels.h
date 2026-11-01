#pragma once
#include "Core/Panel.h"
#include "PianoRollView.h"
#include "raylib.h"

class SequenceEditorSession;

class PianoRollPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }

private:
    void FitInitialView(float viewHeight);
    void HandleInput(const Rectangle& ruler, const Rectangle& grid, bool hover);
    void DrawNotes(const Rectangle& grid, double tickStart, double tickEnd);

    SequenceEditorSession* session = nullptr;
    Starsequence::PianoRollView view;
    std::vector<Starsequence::GridLine> gridLines;
    bool viewFitted = false;
    bool scrubbing = false;
    bool panning = false;
    Vector2 lastMouse = { 0, 0 };
    int lastRevision = -1;
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
