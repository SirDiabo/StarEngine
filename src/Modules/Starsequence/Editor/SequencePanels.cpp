#include "SequencePanels.h"
#include "SequenceEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/KeyLayout.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetFont.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace Starsequence;

namespace {

constexpr float kRulerHeight = 24.0f;
constexpr float kKeyboardWidth = 62.0f;
constexpr float kLaneHeight = 76.0f;
constexpr float kMinHeightForLane = 260.0f;
constexpr float kFontSize = 11.0f;
constexpr float kFontSpacing = 1.0f;
constexpr float kLayerRowHeight = 50.0f;
constexpr float kInstrumentRowHeight = 24.0f;
constexpr float kHeaderHeight = 28.0f;

bool Hover(Rectangle r) {
    const void* scope = WidgetInput::GetInputScope();
    Vector2 m = GetMousePosition();
    if (WidgetInput::IsMouseBlocked()) return false;
    if (scope && WidgetInput::IsMouseOccluded(m, scope)) return false;
    return CheckCollisionPointRec(m, r);
}

void Text(const std::string& text, float x, float y, Color color) {
    DrawTextEx(WidgetFont::Get(), text.c_str(), { x, y }, kFontSize, kFontSpacing, color);
}

float TextWidth(const std::string& text) {
    return MeasureTextEx(WidgetFont::Get(), text.c_str(), kFontSize, kFontSpacing).x;
}

Color InstrumentColor(int instrument, float value) {
    float hue = std::fmod((float)instrument * 137.508f, 360.0f);
    return ColorFromHSV(hue, 0.55f, value);
}

bool ModifierCtrl() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }
bool ModifierShift() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
bool ModifierAlt() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }
bool KeyRepeat(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

bool SmallButton(Rectangle r, const char* label, bool active, Color activeColor) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = Hover(r);
    DrawRectangleRec(r, active ? activeColor : (hovered ? theme.buttonHover : theme.buttonBg));
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    float tw = TextWidth(label);
    Text(label, r.x + (r.width - tw) / 2.0f, r.y + (r.height - kFontSize) / 2.0f, theme.textPrimary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

}

void PianoRollPanel::Initialize() {
    name = "Piano Roll";
}

PianoRollPanel::Layout PianoRollPanel::ComputeLayout() const {
    Layout l;
    l.hasLane = bounds.height >= kMinHeightForLane;
    float laneHeight = l.hasLane ? kLaneHeight : 0.0f;
    l.ruler = { bounds.x + kKeyboardWidth, bounds.y, bounds.width - kKeyboardWidth, kRulerHeight };
    l.keyboard = { bounds.x, bounds.y + kRulerHeight, kKeyboardWidth, bounds.height - kRulerHeight - laneHeight };
    l.grid = { bounds.x + kKeyboardWidth, bounds.y + kRulerHeight, bounds.width - kKeyboardWidth, bounds.height - kRulerHeight - laneHeight };
    l.lane = { bounds.x + kKeyboardWidth, bounds.y + bounds.height - laneHeight, bounds.width - kKeyboardWidth, laneHeight };
    return l;
}

void PianoRollPanel::FitInitialView(float viewHeight) {
    const Sequence& seq = session->GetSequence();
    int lowKey = 127, highKey = 0;
    bool any = false;
    for (const Layer& layer : seq.layers) {
        for (const Note& n : layer.notes) {
            lowKey = std::min(lowKey, n.key);
            highKey = std::max(highKey, n.key);
            any = true;
        }
    }
    view.resolution = seq.resolution;
    view.scrollTick = 0.0;
    if (!any) {
        view.topKey = 84.0;
    } else {
        double visible = (double)viewHeight / (double)view.keyHeight;
        double center = (lowKey + highKey) / 2.0;
        view.topKey = center + visible / 2.0;
    }
    view.ClampVertical(viewHeight);
}

void PianoRollPanel::HandleNavigation(const Layout& layout, bool hover) {
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();
    const Rectangle& grid = layout.grid;

    if (hover && wheel != 0.0f) {
        if (ModifierCtrl()) {
            view.ZoomTime(std::pow(1.15, (double)wheel), mouse.x, grid.x);
        } else if (ModifierAlt()) {
            view.ZoomKeys(std::pow(1.1, (double)wheel), mouse.y, grid.y, grid.height);
        } else if (ModifierShift()) {
            view.ScrollTicks(-(double)wheel * 80.0 / view.PixelsPerTick());
        } else {
            view.ScrollKeys((double)wheel * 3.0, grid.height);
        }
    }

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        panning = true;
        lastMouse = mouse;
    }
    if (panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            view.ScrollTicks(-(double)(mouse.x - lastMouse.x) / view.PixelsPerTick());
            view.ScrollKeys((double)(mouse.y - lastMouse.y) / (double)view.keyHeight, grid.height);
            lastMouse = mouse;
        } else {
            panning = false;
        }
    }

    if (Hover(layout.ruler) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && drag == DragMode::None) scrubbing = true;
    if (scrubbing) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            double tick = view.TickOfX(mouse.x, grid.x);
            session->SetCursorTick((double)session->SnapTick(tick, ModifierAlt()));
        } else {
            scrubbing = false;
        }
    }
}

uint32_t PianoRollPanel::HitNote(const Rectangle& grid, Vector2 mouse, bool& onEdge) const {
    onEdge = false;
    const Layer* layer = session->ActiveLayer();
    if (!layer) return 0;
    int key = view.KeyOfY(mouse.y, grid.y);
    for (size_t i = layer->notes.size(); i-- > 0;) {
        const Note& n = layer->notes[i];
        if (n.key != key) continue;
        float x0 = view.XOfTick((double)n.tick, grid.x);
        float x1 = view.XOfTick((double)(n.tick + n.length), grid.x);
        if (x1 - x0 < 3.0f) x1 = x0 + 3.0f;
        if (mouse.x < x0 || mouse.x > x1) continue;
        float edge = std::clamp((x1 - x0) * 0.4f, 3.0f, 7.0f);
        onEdge = mouse.x >= x1 - edge;
        return n.uid;
    }
    return 0;
}

void PianoRollPanel::HandleGridInput(const Layout& layout, bool hover) {
    Layer* layer = session->ActiveLayer();
    if (!layer) return;
    const Rectangle& grid = layout.grid;
    Vector2 mouse = GetMousePosition();
    const bool bypassSnap = ModifierAlt();
    const bool snapOn = session->SnapIndex() != 0 && !bypassSnap;
    const int64_t step = std::max<int64_t>(1, session->SnapTicks());
    const double mouseTick = view.TickOfX(mouse.x, grid.x);
    const int mouseKey = std::clamp(view.KeyOfY(mouse.y, grid.y), 0, 127);

    if (drag == DragMode::None) {
        if (!(hover && Hover(grid))) return;

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            bool edge = false;
            uint32_t uid = HitNote(grid, mouse, edge);
            if (uid) {
                if (!session->SelectedNotes().count(uid)) session->SetSelectedNotes({ uid });
                session->DeleteSelectedNotes();
            }
            return;
        }

        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || panning || scrubbing) return;

        bool onEdge = false;
        uint32_t uid = HitNote(grid, mouse, onEdge);
        if (uid) {
            std::set<uint32_t> selection = session->SelectedNotes();
            if (ModifierCtrl()) {
                if (selection.count(uid)) selection.erase(uid); else selection.insert(uid);
                session->SetSelectedNotes(selection);
                return;
            }
            if (!selection.count(uid)) {
                selection = { uid };
                session->SetSelectedNotes(selection);
            }
            const Note* note = FindNote(*layer, uid);
            if (!note) return;
            dragSnapshots = SnapshotNotes(*layer, session->SelectedNotes());
            dragNote = uid;
            dragRefTick = note->tick;
            dragRefLength = note->length;
            dragRefKey = note->key;
            dragInstrument = note->instrument;
            dragStartTick = mouseTick;
            dragStartKey = mouseKey;
            lastTickDelta = 0;
            lastKeyDelta = 0;
            lastLengthDelta = 0;
            drag = onEdge ? DragMode::Resize : DragMode::Move;
            if (!onEdge) session->AuditionNote(note->instrument, note->key, note->velocity);
            return;
        }

        if (ModifierShift()) {
            drag = DragMode::Box;
            boxStart = mouse;
            boxBase = ModifierCtrl() ? session->SelectedNotes() : std::set<uint32_t>();
            if (!ModifierCtrl()) session->ClearNoteSelection();
            return;
        }

        int64_t tick = snapOn ? (int64_t)std::floor(std::max(0.0, mouseTick) / (double)step) * step : session->SnapTick(mouseTick, true);
        int64_t length = snapOn ? step : std::max<int64_t>(1, session->GetSequence().resolution / 4);
        uint32_t created = session->AddNoteAt(tick, mouseKey, length);
        if (!created) return;
        session->SetSelectedNotes({ created });
        const Note* note = FindNote(*layer, created);
        if (!note) return;
        dragSnapshots = SnapshotNotes(*layer, { created });
        dragNote = created;
        dragRefTick = note->tick;
        dragRefLength = note->length;
        dragRefKey = note->key;
        dragInstrument = note->instrument;
        dragStartTick = mouseTick;
        dragStartKey = mouseKey;
        lastTickDelta = 0;
        lastKeyDelta = 0;
        lastLengthDelta = 0;
        drag = DragMode::Create;
        session->AuditionNote(note->instrument, note->key, note->velocity);
        return;
    }

    if (drag == DragMode::Velocity) return;

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        drag = DragMode::None;
        dragSnapshots.clear();
        return;
    }

    if (drag == DragMode::Move) {
        int64_t newTick = session->SnapTick((double)dragRefTick + (mouseTick - dragStartTick), bypassSnap);
        int64_t tickDelta = newTick - dragRefTick;
        int keyDelta = mouseKey - dragStartKey;
        if (tickDelta != lastTickDelta || keyDelta != lastKeyDelta) {
            ApplyMove(*layer, dragSnapshots, tickDelta, keyDelta);
            session->NotesEdited();
            if (keyDelta != lastKeyDelta) session->AuditionNote(dragInstrument, std::clamp(dragRefKey + keyDelta, 0, 127), session->defaultVelocity);
            lastTickDelta = tickDelta;
            lastKeyDelta = keyDelta;
        }
    } else if (drag == DragMode::Resize || drag == DragMode::Create) {
        int64_t refEnd = dragRefTick + dragRefLength;
        int64_t newEnd;
        if (drag == DragMode::Create && snapOn) newEnd = (int64_t)std::ceil(std::max(0.0, mouseTick) / (double)step) * step;
        else newEnd = session->SnapTick(mouseTick, bypassSnap);
        int64_t lengthDelta = newEnd - refEnd;
        if (lengthDelta != lastLengthDelta) {
            ApplyResize(*layer, dragSnapshots, lengthDelta, snapOn ? step : 1);
            session->NotesEdited();
            lastLengthDelta = lengthDelta;
        }
    } else if (drag == DragMode::Box) {
        Rectangle box = {
            std::min(boxStart.x, mouse.x), std::min(boxStart.y, mouse.y),
            std::fabs(mouse.x - boxStart.x), std::fabs(mouse.y - boxStart.y)
        };
        std::set<uint32_t> selection = boxBase;
        for (const Note& n : layer->notes) {
            float x0 = view.XOfTick((double)n.tick, grid.x);
            float x1 = std::max(x0 + 2.0f, view.XOfTick((double)(n.tick + n.length), grid.x));
            float y0 = view.YOfKey(n.key, grid.y);
            Rectangle nr = { x0, y0, x1 - x0, view.keyHeight };
            if (CheckCollisionRecs(box, nr)) selection.insert(n.uid);
        }
        session->SetSelectedNotes(selection);
    }
}

void PianoRollPanel::HandleLaneInput(const Layout& layout, bool hover) {
    if (!layout.hasLane) return;
    Layer* layer = session->ActiveLayer();
    if (!layer) return;
    Vector2 mouse = GetMousePosition();
    const float usable = layout.lane.height - 10.0f;
    const float bottom = layout.lane.y + layout.lane.height - 4.0f;

    if (drag == DragMode::Velocity) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            drag = DragMode::None;
            velocityTargets.clear();
            return;
        }
        int velocity = (int)std::lround((double)(bottom - mouse.y) / (double)usable * 127.0);
        velocity = std::clamp(velocity, 1, 127);
        ApplyVelocity(*layer, velocityTargets, velocity);
        session->defaultVelocity = velocity;
        session->MarkStructureChanged();
        return;
    }

    if (drag != DragMode::None || !hover || !Hover(layout.lane) || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    uint32_t best = 0;
    float bestDistance = 7.0f;
    for (const Note& n : layer->notes) {
        float x = view.XOfTick((double)n.tick, layout.lane.x);
        float distance = std::fabs(x - mouse.x);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = n.uid;
        }
    }
    if (!best) return;
    std::set<uint32_t> selection = session->SelectedNotes();
    if (!selection.count(best)) {
        selection = { best };
        session->SetSelectedNotes(selection);
    }
    velocityTargets = selection;
    drag = DragMode::Velocity;
}

void PianoRollPanel::HandleKeyboardColumn(const Layout& layout, bool hover) {
    if (!hover || drag != DragMode::None || !Hover(layout.keyboard) || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    int key = std::clamp(view.KeyOfY(GetMousePosition().y, layout.keyboard.y), 0, 127);
    session->AuditionNote(session->CurrentInstrument(), key, session->defaultVelocity);
}

void PianoRollPanel::HandleShortcuts(bool hover) {
    if (!hover || drag != DragMode::None) return;
    if (WidgetInput::IsKeyboardCaptured() || WidgetInput::HasModal(WidgetInput::kEditorDevice)) return;
    Layer* layer = session->ActiveLayer();
    if (!layer) return;

    const bool ctrl = ModifierCtrl();
    if (ctrl) {
        if (KeyLayout::Pressed('a')) session->SelectAllNotes();
        else if (KeyLayout::Pressed('c')) session->CopySelection();
        else if (KeyLayout::Pressed('x')) session->CutSelection();
        else if (KeyLayout::Pressed('v')) session->Paste();
        else if (KeyLayout::Pressed('d')) session->DuplicateSelection();
        return;
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        session->DeleteSelectedNotes();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        session->ClearNoteSelection();
        return;
    }
    if (session->SelectedNotes().empty()) return;

    int64_t tickDelta = 0;
    int keyDelta = 0;
    const int64_t step = std::max<int64_t>(1, session->SnapTicks());
    if (KeyRepeat(KEY_LEFT)) tickDelta = -step;
    else if (KeyRepeat(KEY_RIGHT)) tickDelta = step;
    else if (KeyRepeat(KEY_UP)) keyDelta = ModifierShift() ? 12 : 1;
    else if (KeyRepeat(KEY_DOWN)) keyDelta = ModifierShift() ? -12 : -1;
    if (tickDelta == 0 && keyDelta == 0) return;

    std::vector<NoteSnapshot> snapshots = SnapshotNotes(*layer, session->SelectedNotes());
    ApplyMove(*layer, snapshots, tickDelta, keyDelta);
    session->NotesEdited();
    if (keyDelta != 0 && !snapshots.empty()) {
        const Note* n = FindNote(*layer, snapshots.front().uid);
        if (n) session->AuditionNote(n->instrument, n->key, n->velocity);
    }
}

void PianoRollPanel::DrawNotes(const Rectangle& grid, double tickStart, double tickEnd) {
    const Sequence& seq = session->GetSequence();
    const int selected = session->SelectedLayer();
    for (size_t l = 0; l < seq.layers.size(); l++) {
        if ((int)l == selected) continue;
        const Layer& layer = seq.layers[l];
        for (const Note& n : layer.notes) {
            if ((double)n.tick > tickEnd) break;
            if ((double)(n.tick + n.length) < tickStart) continue;
            float x0 = view.XOfTick((double)n.tick, grid.x);
            float x1 = view.XOfTick((double)(n.tick + n.length), grid.x);
            float y = view.YOfKey(n.key, grid.y);
            if (y + view.keyHeight < grid.y || y > grid.y + grid.height) continue;
            Rectangle r = { x0, y + 2.0f, std::max(2.0f, x1 - x0), std::max(2.0f, view.keyHeight - 4.0f) };
            DrawRectangleRec(r, Fade(InstrumentColor(n.instrument, 0.8f), layer.muted ? 0.10f : 0.22f));
        }
    }
    if (selected < 0 || (size_t)selected >= seq.layers.size()) return;
    const Layer& layer = seq.layers[(size_t)selected];
    const std::set<uint32_t>& selection = session->SelectedNotes();
    for (const Note& n : layer.notes) {
        if ((double)n.tick > tickEnd) break;
        if ((double)(n.tick + n.length) < tickStart) continue;
        float x0 = view.XOfTick((double)n.tick, grid.x);
        float x1 = view.XOfTick((double)(n.tick + n.length), grid.x);
        float y = view.YOfKey(n.key, grid.y);
        if (y + view.keyHeight < grid.y || y > grid.y + grid.height) continue;
        float value = 0.5f + 0.5f * (float)std::clamp(n.velocity, 1, 127) / 127.0f;
        Rectangle r = { x0, y + 1.0f, std::max(2.0f, x1 - x0), std::max(2.0f, view.keyHeight - 2.0f) };
        Color c = InstrumentColor(n.instrument, value);
        DrawRectangleRec(r, layer.muted ? Fade(c, 0.5f) : c);
        bool isSelected = selection.count(n.uid) > 0;
        DrawRectangleLinesEx(r, isSelected ? 2.0f : 1.0f, isSelected ? WHITE : Fade(BLACK, 0.45f));
        if (r.width > 10.0f) DrawRectangleRec({ r.x + r.width - 3.0f, r.y, 3.0f, r.height }, Fade(BLACK, 0.25f));
    }
}

void PianoRollPanel::DrawVelocityLane(const Layout& layout, double tickStart, double tickEnd) {
    if (!layout.hasLane) return;
    const UIPalette& theme = UITheme::Get();
    const Rectangle& lane = layout.lane;
    DrawRectangleRec(lane, ColorBrightness(theme.panelBg, -0.1f));
    DrawLineEx({ lane.x, lane.y }, { lane.x + lane.width, lane.y }, 1.0f, theme.panelBorder);
    DrawRectangleRec({ bounds.x, lane.y, kKeyboardWidth, lane.height }, theme.panelBg);
    Text("Velocity", bounds.x + 6.0f, lane.y + 6.0f, theme.textSecondary);
    DrawLineEx({ lane.x, lane.y }, { lane.x, lane.y + lane.height }, 1.0f, theme.panelBorder);

    for (const GridLine& line : gridLines) {
        if (line.kind == 0) continue;
        float x = view.XOfTick((double)line.tick, lane.x);
        if (x < lane.x || x > lane.x + lane.width) continue;
        DrawLineEx({ x, lane.y }, { x, lane.y + lane.height }, 1.0f, Fade(theme.textSecondary, line.kind == 2 ? 0.35f : 0.15f));
    }

    const Layer* layer = session->ActiveLayer();
    if (!layer) return;
    const float usable = lane.height - 10.0f;
    const float bottom = lane.y + lane.height - 4.0f;
    const std::set<uint32_t>& selection = session->SelectedNotes();
    for (const Note& n : layer->notes) {
        if ((double)n.tick > tickEnd) break;
        if ((double)(n.tick + n.length) < tickStart) continue;
        float x = view.XOfTick((double)n.tick, lane.x);
        if (x < lane.x - 4.0f || x > lane.x + lane.width + 4.0f) continue;
        float h = usable * (float)n.velocity / 127.0f;
        bool isSelected = selection.count(n.uid) > 0;
        Color c = InstrumentColor(n.instrument, 0.9f);
        DrawRectangleRec({ x - 1.0f, bottom - h, 3.0f, h }, isSelected ? WHITE : Fade(c, 0.85f));
        DrawCircleV({ x + 0.5f, bottom - h }, 3.5f, isSelected ? WHITE : c);
    }
}

void PianoRollPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    const Sequence& seq = session->GetSequence();

    Layout layout = ComputeLayout();
    const Rectangle& ruler = layout.ruler;
    const Rectangle& keyboard = layout.keyboard;
    const Rectangle& grid = layout.grid;
    if (grid.width < 8 || grid.height < 8) return;

    if (!viewFitted) {
        FitInitialView(grid.height);
        viewFitted = true;
    }
    view.resolution = seq.resolution;
    view.ClampVertical(grid.height);

    bool hover = Hover(bounds);
    HandleNavigation(layout, hover);
    HandleLaneInput(layout, hover);
    HandleGridInput(layout, hover);
    HandleKeyboardColumn(layout, hover);
    HandleShortcuts(hover);

    const double playhead = session->PlayheadTick();
    if (session->IsPlaying() && !scrubbing) {
        float px = view.XOfTick(playhead, grid.x);
        if (px > grid.x + grid.width - 20.0f || px < grid.x) {
            double visibleTicks = (double)grid.width / view.PixelsPerTick();
            view.scrollTick = std::max(0.0, playhead - visibleTicks * 0.1);
        }
    }

    const double tickStart = view.TickOfX(grid.x, grid.x);
    const double tickEnd = view.TickOfX(grid.x + grid.width, grid.x);

    DrawRectangleRec(bounds, theme.panelBg);
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);

    int firstKey = std::min(127, view.KeyOfY(grid.y, grid.y));
    int lastKey = std::max(0, view.KeyOfY(grid.y + grid.height, grid.y));
    for (int key = firstKey; key >= lastKey; key--) {
        float y = view.YOfKey(key, grid.y);
        Color row = IsBlackKey(key) ? ColorBrightness(theme.panelBg, -0.18f) : ColorBrightness(theme.panelBg, 0.04f);
        if (key == view.KeyOfY(GetMousePosition().y, grid.y) && Hover(grid)) row = ColorBrightness(row, 0.12f);
        DrawRectangleRec({ grid.x, y, grid.width, view.keyHeight }, row);
        if (key % 12 == 0) DrawLineEx({ grid.x, y + view.keyHeight }, { grid.x + grid.width, y + view.keyHeight }, 1.0f, Fade(theme.textSecondary, 0.35f));
    }

    int subdivision = AutoSubdivision(view.pixelsPerBeat);
    BuildGridLines(seq, tickStart, tickEnd, subdivision, gridLines);
    for (const GridLine& line : gridLines) {
        float x = view.XOfTick((double)line.tick, grid.x);
        if (x < grid.x || x > grid.x + grid.width) continue;
        float alpha = line.kind == 2 ? 0.55f : (line.kind == 1 ? 0.28f : 0.12f);
        DrawLineEx({ x, grid.y }, { x, grid.y + grid.height }, 1.0f, Fade(theme.textSecondary, alpha));
    }

    float endX = view.XOfTick((double)seq.length, grid.x);
    if (endX < grid.x + grid.width) {
        float from = std::max(grid.x, endX);
        DrawRectangleRec({ from, grid.y, grid.x + grid.width - from, grid.height }, Fade(BLACK, 0.35f));
        if (endX >= grid.x) DrawLineEx({ endX, grid.y }, { endX, grid.y + grid.height }, 2.0f, Fade(theme.danger, 0.7f));
    }

    DrawNotes(grid, tickStart, tickEnd);

    for (const Marker& m : seq.markers) {
        float x = view.XOfTick((double)m.tick, grid.x);
        if (x < grid.x || x > grid.x + grid.width) continue;
        DrawLineEx({ x, grid.y }, { x, grid.y + grid.height }, 1.0f, Fade(theme.success, 0.6f));
    }

    if (drag == DragMode::Box) {
        Vector2 mouse = GetMousePosition();
        Rectangle box = { std::min(boxStart.x, mouse.x), std::min(boxStart.y, mouse.y), std::fabs(mouse.x - boxStart.x), std::fabs(mouse.y - boxStart.y) };
        DrawRectangleRec(box, Fade(theme.accent, 0.2f));
        DrawRectangleLinesEx(box, 1, theme.accent);
    }

    DrawRectangleRec(ruler, ColorBrightness(theme.panelBg, -0.08f));
    int64_t loopA = seq.loopEnd > seq.loopStart ? seq.loopStart : 0;
    int64_t loopB = seq.loopEnd > seq.loopStart ? seq.loopEnd : seq.length;
    {
        float x0 = std::max(ruler.x, view.XOfTick((double)loopA, grid.x));
        float x1 = std::min(ruler.x + ruler.width, view.XOfTick((double)loopB, grid.x));
        if (x1 > x0) DrawRectangleRec({ x0, ruler.y + ruler.height - 5.0f, x1 - x0, 5.0f }, Fade(theme.accent, session->loopEnabled ? 0.85f : 0.35f));
    }
    for (const GridLine& line : gridLines) {
        if (line.kind == 0) continue;
        float x = view.XOfTick((double)line.tick, grid.x);
        if (x < ruler.x || x > ruler.x + ruler.width) continue;
        if (line.kind == 2) {
            DrawLineEx({ x, ruler.y + 3.0f }, { x, ruler.y + ruler.height - 6.0f }, 1.0f, theme.textSecondary);
            Text(std::to_string(line.bar), x + 3.0f, ruler.y + 3.0f, theme.textPrimary);
        } else if (view.pixelsPerBeat >= 30.0) {
            DrawLineEx({ x, ruler.y + 14.0f }, { x, ruler.y + ruler.height - 6.0f }, 1.0f, Fade(theme.textSecondary, 0.6f));
        }
    }
    for (const Marker& m : seq.markers) {
        float x = view.XOfTick((double)m.tick, grid.x);
        if (x < ruler.x - 200.0f || x > ruler.x + ruler.width) continue;
        DrawRectangleRec({ x, ruler.y, 2.0f, ruler.height - 5.0f }, theme.success);
        Text(m.name, x + 5.0f, ruler.y + 11.0f, theme.success);
    }
    DrawLineEx({ ruler.x, ruler.y + ruler.height }, { ruler.x + ruler.width, ruler.y + ruler.height }, 1.0f, theme.panelBorder);

    float cursorX = view.XOfTick(session->CursorTick(), grid.x);
    if (cursorX >= grid.x && cursorX <= grid.x + grid.width && !session->IsPlaying()) {
        DrawLineEx({ cursorX, ruler.y }, { cursorX, grid.y + grid.height }, 1.0f, Fade(theme.accent, 0.8f));
    }
    float playX = view.XOfTick(playhead, grid.x);
    if (playX >= grid.x && playX <= grid.x + grid.width) {
        DrawLineEx({ playX, ruler.y }, { playX, grid.y + grid.height }, 2.0f, theme.accent);
        DrawTriangle({ playX - 5.0f, ruler.y }, { playX + 5.0f, ruler.y }, { playX, ruler.y + 8.0f }, theme.accent);
    }

    DrawRectangleRec(keyboard, theme.panelBg);
    for (int key = firstKey; key >= lastKey; key--) {
        float y = view.YOfKey(key, keyboard.y);
        bool black = IsBlackKey(key);
        Rectangle k = { keyboard.x, y, black ? kKeyboardWidth * 0.62f : kKeyboardWidth, view.keyHeight };
        if (!black) DrawRectangleRec({ keyboard.x, y, kKeyboardWidth, view.keyHeight }, ColorBrightness(theme.panelBg, 0.55f));
        DrawRectangleRec(k, black ? ColorBrightness(theme.panelBg, -0.55f) : ColorBrightness(theme.panelBg, 0.55f));
        DrawLineEx({ keyboard.x, y + view.keyHeight }, { keyboard.x + kKeyboardWidth, y + view.keyHeight }, 1.0f, Fade(BLACK, 0.35f));
        if (key % 12 == 0 && view.keyHeight >= 9.0f) {
            int octave = 0;
            const char* noteName = KeyName(key, octave);
            Text(std::string(noteName) + std::to_string(octave), keyboard.x + kKeyboardWidth - 28.0f, y + (view.keyHeight - kFontSize) / 2.0f, BLACK);
        }
    }
    DrawRectangleRec({ bounds.x, bounds.y, kKeyboardWidth, kRulerHeight }, ColorBrightness(theme.panelBg, -0.08f));
    DrawRectangleLinesEx({ bounds.x, bounds.y, kKeyboardWidth, kRulerHeight }, 1, theme.panelBorder);
    DrawLineEx({ keyboard.x + kKeyboardWidth, keyboard.y }, { keyboard.x + kKeyboardWidth, keyboard.y + keyboard.height }, 1.0f, theme.panelBorder);

    DrawVelocityLane(layout, tickStart, tickEnd);

    EndScissorMode();
}

void SequenceLayersPanel::Initialize() {
    name = "Layers";
}

void SequenceLayersPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    const Sequence& seq = session->GetSequence();

    DrawRectangleRec(bounds, theme.panelBg);
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);

    float buttonWidth = (bounds.width - 16.0f - 8.0f) / 3.0f;
    Rectangle addBtn = { bounds.x + 4.0f, bounds.y + 4.0f, buttonWidth, kHeaderHeight - 8.0f };
    Rectangle dupBtn = { addBtn.x + buttonWidth + 4.0f, addBtn.y, buttonWidth, addBtn.height };
    Rectangle delBtn = { dupBtn.x + buttonWidth + 4.0f, addBtn.y, buttonWidth, addBtn.height };
    bool headerClicked = false;
    if (SmallButton(addBtn, "+ Layer", false, theme.accent)) { session->AddLayer(); headerClicked = true; }
    if (SmallButton(dupBtn, "Duplicate", false, theme.accent)) { session->DuplicateSelectedLayer(); headerClicked = true; }
    if (SmallButton(delBtn, "Remove", false, theme.danger)) { session->RemoveSelectedLayer(); headerClicked = true; }
    DrawLineEx({ bounds.x, bounds.y + kHeaderHeight }, { bounds.x + bounds.width, bounds.y + kHeaderHeight }, 1.0f, theme.panelBorder);

    Rectangle listArea = { bounds.x, bounds.y + kHeaderHeight, bounds.width, bounds.height - kHeaderHeight };
    float contentHeight = (float)seq.layers.size() * kLayerRowHeight;
    float maxScroll = std::max(0.0f, contentHeight - listArea.height);
    if (Hover(listArea)) scroll -= GetMouseWheelMove() * 30.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    Vector2 mouse = GetMousePosition();
    if (dragLayer >= 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragLayer = -1;

    for (size_t l = 0; l < seq.layers.size(); l++) {
        const Layer& layer = seq.layers[l];
        const int index = (int)l;
        Rectangle row = { listArea.x, listArea.y + (float)l * kLayerRowHeight - scroll, listArea.width, kLayerRowHeight };
        if (row.y + row.height < listArea.y || row.y > listArea.y + listArea.height) continue;
        bool rowHover = Hover(row) && Hover(listArea);

        bool selected = index == session->SelectedLayer();
        DrawRectangleRec(row, selected ? theme.surfaceSelected : (rowHover ? theme.surfaceHover : theme.panelBg));
        DrawLineEx({ row.x, row.y + row.height }, { row.x + row.width, row.y + row.height }, 1.0f, theme.panelBorder);

        int firstInstrument = 0;
        for (const Note& n : layer.notes) { firstInstrument = n.instrument; break; }
        DrawRectangleRec({ row.x, row.y, 4.0f, row.height }, InstrumentColor(firstInstrument, 0.85f));

        Rectangle muteBtn = { row.x + row.width - 46.0f, row.y + 5.0f, 20.0f, 18.0f };
        Rectangle soloBtn = { row.x + row.width - 24.0f, row.y + 5.0f, 20.0f, 18.0f };
        float nameMax = muteBtn.x - (row.x + 12.0f) - 4.0f;
        std::string label = layer.name;
        while (label.size() > 1 && TextWidth(label) > nameMax) label.pop_back();
        Text(label, row.x + 12.0f, row.y + 7.0f, layer.muted ? theme.textSecondary : theme.textPrimary);

        bool clickedButton = false;
        if (SmallButton(muteBtn, "M", layer.muted, theme.danger)) { session->SetLayerMuted(index, !layer.muted); clickedButton = true; }
        if (SmallButton(soloBtn, "S", session->IsLayerSolo(index), theme.accent)) { session->SetLayerSolo(index, !session->IsLayerSolo(index)); clickedButton = true; }

        float half = (row.width - 24.0f) / 2.0f - 4.0f;
        Rectangle volTrack = { row.x + 12.0f, row.y + 33.0f, half, 8.0f };
        Rectangle panTrack = { row.x + 12.0f + half + 8.0f, row.y + 33.0f, half, 8.0f };
        Rectangle volHit = { volTrack.x, row.y + 28.0f, volTrack.width, 18.0f };
        Rectangle panHit = { panTrack.x, row.y + 28.0f, panTrack.width, 18.0f };

        if (Hover(volHit) && Hover(listArea) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { dragLayer = index; dragKind = 1; clickedButton = true; }
        if (Hover(panHit) && Hover(listArea) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { dragLayer = index; dragKind = 2; clickedButton = true; }
        if (dragLayer == index) {
            if (dragKind == 1) session->SetLayerVolume(index, std::clamp((mouse.x - volTrack.x) / volTrack.width, 0.0f, 1.0f) * 2.0f);
            else session->SetLayerPan(index, std::clamp((mouse.x - panTrack.x) / panTrack.width, 0.0f, 1.0f) * 2.0f - 1.0f);
        }

        DrawRectangleRec(volTrack, theme.fieldBg);
        DrawRectangleRec({ volTrack.x, volTrack.y, volTrack.width * std::clamp(layer.volume / 2.0f, 0.0f, 1.0f), volTrack.height }, Fade(theme.accent, 0.8f));
        DrawRectangleLinesEx(volTrack, 1, theme.fieldBorder);
        DrawRectangleRec(panTrack, theme.fieldBg);
        float panX = panTrack.x + panTrack.width * (layer.pan * 0.5f + 0.5f);
        DrawRectangleRec({ panTrack.x + panTrack.width / 2.0f - 0.5f, panTrack.y, 1.0f, panTrack.height }, theme.fieldBorder);
        DrawRectangleRec({ panX - 2.0f, panTrack.y - 1.0f, 4.0f, panTrack.height + 2.0f }, theme.accent);
        DrawRectangleLinesEx(panTrack, 1, theme.fieldBorder);

        char info[64];
        snprintf(info, sizeof(info), "%d notes", (int)layer.notes.size());
        Text(info, row.x + 12.0f, row.y + 19.0f, theme.textSecondary);

        if (!clickedButton && !headerClicked && rowHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) session->SelectLayer(index);
    }

    EndScissorMode();
}

void SequenceInstrumentsPanel::Initialize() {
    name = "Instruments";
}

void SequenceInstrumentsPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    const Sequence& seq = session->GetSequence();

    DrawRectangleRec(bounds, theme.panelBg);
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);

    float buttonWidth = (bounds.width - 12.0f - 4.0f) / 2.0f;
    Rectangle addBtn = { bounds.x + 4.0f, bounds.y + 4.0f, buttonWidth, kHeaderHeight - 8.0f };
    Rectangle delBtn = { addBtn.x + buttonWidth + 4.0f, addBtn.y, buttonWidth, addBtn.height };
    bool headerClicked = false;
    if (SmallButton(addBtn, "+ Instrument", false, theme.accent)) { session->AddInstrument(); headerClicked = true; }
    if (SmallButton(delBtn, "Remove", false, theme.danger)) { session->RemoveInstrument(session->CurrentInstrument()); headerClicked = true; }
    DrawLineEx({ bounds.x, bounds.y + kHeaderHeight }, { bounds.x + bounds.width, bounds.y + kHeaderHeight }, 1.0f, theme.panelBorder);

    Rectangle listArea = { bounds.x, bounds.y + kHeaderHeight, bounds.width, bounds.height - kHeaderHeight };
    float contentHeight = (float)seq.instruments.size() * kInstrumentRowHeight;
    float maxScroll = std::max(0.0f, contentHeight - listArea.height);
    if (Hover(listArea)) scroll -= GetMouseWheelMove() * 30.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    for (size_t i = 0; i < seq.instruments.size(); i++) {
        const Instrument& inst = seq.instruments[i];
        const int index = (int)i;
        Rectangle row = { listArea.x, listArea.y + (float)i * kInstrumentRowHeight - scroll, listArea.width, kInstrumentRowHeight };
        if (row.y + row.height < listArea.y || row.y > listArea.y + listArea.height) continue;
        bool rowHover = Hover(row) && Hover(listArea);
        bool selected = index == session->CurrentInstrument();
        DrawRectangleRec(row, selected ? theme.surfaceSelected : (rowHover ? theme.surfaceHover : theme.panelBg));
        DrawLineEx({ row.x, row.y + row.height }, { row.x + row.width, row.y + row.height }, 1.0f, theme.panelBorder);
        DrawRectangleRec({ row.x, row.y, 4.0f, row.height }, InstrumentColor(index, 0.85f));

        int zones = 0;
        for (const SampleSet& set : inst.sets) zones += (int)set.zones.size();
        char count[32];
        snprintf(count, sizeof(count), "%d", zones);
        float countWidth = TextWidth(count);
        Text(count, row.x + row.width - countWidth - 8.0f, row.y + (row.height - kFontSize) / 2.0f, zones == 0 ? theme.danger : theme.textSecondary);

        std::string label = std::to_string(index) + "  " + inst.name;
        float nameMax = row.width - countWidth - 28.0f;
        while (label.size() > 1 && TextWidth(label) > nameMax) label.pop_back();
        Text(label, row.x + 12.0f, row.y + (row.height - kFontSize) / 2.0f, theme.textPrimary);

        if (!headerClicked && rowHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            session->SetCurrentInstrument(index);
            session->AuditionNote(index, 60, session->defaultVelocity);
        }
    }

    EndScissorMode();
}
