#include "SequencePanels.h"
#include "SequenceEditorSession.h"
#include "Editor/UITheme.h"
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
constexpr float kFontSize = 11.0f;
constexpr float kFontSpacing = 1.0f;
constexpr float kLayerRowHeight = 50.0f;

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

void PianoRollPanel::HandleInput(const Rectangle& ruler, const Rectangle& grid, bool hover) {
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();

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

    if (Hover(ruler) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) scrubbing = true;
    if (scrubbing) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            double tick = view.TickOfX(mouse.x, grid.x);
            session->SetCursorTick(std::round(std::max(0.0, tick)));
        } else {
            scrubbing = false;
        }
    }
}

void PianoRollPanel::DrawNotes(const Rectangle& grid, double tickStart, double tickEnd) {
    const Sequence& seq = session->GetSequence();
    const int selected = session->SelectedLayer();
    for (size_t l = 0; l < seq.layers.size(); l++) {
        const Layer& layer = seq.layers[l];
        const bool isSelected = (int)l == selected;
        if (isSelected) continue;
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
        DrawRectangleLinesEx(r, 1, Fade(BLACK, 0.45f));
    }
}

void PianoRollPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    const Sequence& seq = session->GetSequence();

    Rectangle ruler = { bounds.x + kKeyboardWidth, bounds.y, bounds.width - kKeyboardWidth, kRulerHeight };
    Rectangle keyboard = { bounds.x, bounds.y + kRulerHeight, kKeyboardWidth, bounds.height - kRulerHeight };
    Rectangle grid = { bounds.x + kKeyboardWidth, bounds.y + kRulerHeight, bounds.width - kKeyboardWidth, bounds.height - kRulerHeight };
    if (grid.width < 8 || grid.height < 8) return;

    if (!viewFitted) {
        FitInitialView(grid.height);
        viewFitted = true;
    }
    view.resolution = seq.resolution;
    view.ClampVertical(grid.height);

    bool hover = Hover(bounds);
    HandleInput(ruler, grid, hover);

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

    float contentHeight = (float)seq.layers.size() * kLayerRowHeight;
    float maxScroll = std::max(0.0f, contentHeight - bounds.height);
    if (Hover(bounds)) scroll -= GetMouseWheelMove() * 30.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    Vector2 mouse = GetMousePosition();
    if (dragLayer >= 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragLayer = -1;

    for (size_t l = 0; l < seq.layers.size(); l++) {
        const Layer& layer = seq.layers[l];
        const int index = (int)l;
        Rectangle row = { bounds.x, bounds.y + (float)l * kLayerRowHeight - scroll, bounds.width, kLayerRowHeight };
        if (row.y + row.height < bounds.y || row.y > bounds.y + bounds.height) continue;

        bool selected = index == session->SelectedLayer();
        DrawRectangleRec(row, selected ? theme.surfaceSelected : (Hover(row) ? theme.surfaceHover : theme.panelBg));
        DrawLineEx({ row.x, row.y + row.height }, { row.x + row.width, row.y + row.height }, 1.0f, theme.panelBorder);

        int firstInstrument = -1;
        for (const Note& n : layer.notes) { firstInstrument = n.instrument; break; }
        DrawRectangleRec({ row.x, row.y, 4.0f, row.height }, InstrumentColor(std::max(0, firstInstrument), 0.85f));

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

        if (Hover(volHit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { dragLayer = index; dragKind = 1; clickedButton = true; }
        if (Hover(panHit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { dragLayer = index; dragKind = 2; clickedButton = true; }
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

        if (!clickedButton && Hover(row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) session->SelectLayer(index);
    }

    EndScissorMode();
}
