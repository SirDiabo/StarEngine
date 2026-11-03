#include "InstrumentEditorPanel.h"
#include "SequenceEditorSession.h"
#include "Modules/Starsequence/SequenceEdit.h"
#include "Modules/Starsequence/SequenceAudioSource.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/ScissorStack.h"
#include "Core/Widgets/WidgetFont.h"
#include "Core/Widgets/WidgetInput.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace Starsequence;

namespace {

constexpr float kFontSize = 11.0f;
constexpr float kSmallFontSize = 10.0f;
constexpr float kFontSpacing = 1.0f;
constexpr float kPad = 12.0f;

void Text(const std::string& text, float x, float y, Color color, float size = kFontSize) {
    DrawTextEx(WidgetFont::Get(), text.c_str(), { x, y }, size, kFontSpacing, color);
}

float TextWidth(const std::string& text, float size = kFontSize) {
    return MeasureTextEx(WidgetFont::Get(), text.c_str(), size, kFontSpacing).x;
}

std::string Fit(const std::string& text, float width) {
    if (TextWidth(text) <= width) return text;
    std::string cut = text;
    while (cut.size() > 1 && TextWidth(cut + "...") > width) cut.pop_back();
    return cut + "...";
}

std::string FormatNumber(float value, int decimals, bool integer) {
    char buffer[40];
    if (integer) {
        snprintf(buffer, sizeof(buffer), "%d", (int)std::lround(value));
        return buffer;
    }
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    std::string s = buffer;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() || s == "-" ? "0" : s;
}

void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) DrawTriangle(a, c, b, color);
    else DrawTriangle(a, b, c, color);
}

Color ZoneColor(int index, float value = 0.85f) {
    return ColorFromHSV(std::fmod((float)index * 47.0f, 360.0f), 0.55f, value);
}

const char* LoopLabel(LoopMode mode) {
    switch (mode) {
        case LoopMode::Whole: return "Loop: Whole";
        case LoopMode::Region: return "Loop: Region";
        case LoopMode::Wav: return "Loop: From WAV";
        default: return "Loop: Off";
    }
}

LoopMode NextLoop(LoopMode mode) {
    switch (mode) {
        case LoopMode::None: return LoopMode::Whole;
        case LoopMode::Whole: return LoopMode::Region;
        case LoopMode::Region: return LoopMode::Wav;
        default: return LoopMode::None;
    }
}

}

void InstrumentEditorPanel::Initialize() {
    name = "Instrument Editor";
    visible = false;
}

void InstrumentEditorPanel::Open() {
    visible = true;
    scroll = 0.0f;
    activeField.clear();
    selectedZone = -1;
    waveCache.clear();
    loopDragId.clear();
    if (loopPreviewHeld && session) session->EndHeldAudition();
    loopPreviewHeld = false;
}

const InstrumentEditorPanel::ZoneWave* InstrumentEditorPanel::GetWave(const std::string& assetPath) {
    if (assetPath.empty()) return nullptr;
    auto found = waveCache.find(assetPath);
    if (found != waveCache.end()) {
        double now = GetTime();
        double interval = found->second.ok ? 0.5 : 1.0;
        if (now - found->second.checkedAt > interval) {
            found->second.checkedAt = now;
            std::error_code ec;
            auto stamp = std::filesystem::last_write_time(assetPath, ec);
            if (!found->second.ok || (!ec && stamp != found->second.stamp)) {
                waveCache.erase(found);
                found = waveCache.end();
            }
        }
    }
    if (found != waveCache.end()) return found->second.ok ? &found->second : nullptr;
    if (wavesBuiltThisFrame >= 2) return nullptr;
    wavesBuiltThisFrame++;

    ZoneWave wave;
    {
        std::error_code ec;
        wave.stamp = std::filesystem::last_write_time(assetPath, ec);
        wave.checkedAt = GetTime();
    }
    Starsequence::LoadedSample loaded;
    if (Starsequence::LoadStarsoundSample(assetPath, loaded) && loaded.data && loaded.data->frameCount > 0) {
        const SampleData& data = *loaded.data;
        const int buckets = 1024;
        const size_t channels = (size_t)std::max(1, data.channels);
        wave.frames = data.frameCount;
        wave.peakMin.assign((size_t)buckets, 0.0f);
        wave.peakMax.assign((size_t)buckets, 0.0f);
        for (int b = 0; b < buckets; b++) {
            uint64_t start = data.frameCount * (uint64_t)b / (uint64_t)buckets;
            uint64_t end = data.frameCount * (uint64_t)(b + 1) / (uint64_t)buckets;
            if (end <= start) end = std::min<uint64_t>(start + 1, data.frameCount);
            float lo = 0.0f, hi = 0.0f;
            for (uint64_t f = start; f < end; f++) {
                size_t base = (size_t)f * channels;
                if (base + channels > data.frames.size()) break;
                for (size_t c = 0; c < channels; c++) {
                    float v = data.frames[base + c];
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            wave.peakMin[(size_t)b] = lo;
            wave.peakMax[(size_t)b] = hi;
        }
        wave.hasLoop = data.hasLoop;
        wave.wavLoopStart = data.loopStart;
        wave.wavLoopEnd = data.loopEnd;
        wave.ok = true;
    }
    auto inserted = waveCache.emplace(assetPath, std::move(wave));
    return inserted.first->second.ok ? &inserted.first->second : nullptr;
}

bool InstrumentEditorPanel::DrawZoneWave(Rectangle r, Starsequence::SampleZone& z, int zoneIndex, const std::string& id, int instrument, int setIndex) {
    const UIPalette& theme = UITheme::Get();
    bool edited = false;
    DrawRectangleRec(r, theme.fieldBg);
    DrawRectangleLinesEx(r, 1, theme.fieldBorder);

    const ZoneWave* wave = GetWave(z.assetPath);
    if (!wave) {
        const char* msg = z.assetPath.empty() ? "No sound selected" : (waveCache.count(z.assetPath) ? "Could not load sound" : "Loading waveform...");
        Text(msg, r.x + 8.0f, r.y + (r.height - kSmallFontSize) * 0.5f, theme.textSecondary, kSmallFontSize);
        return false;
    }

    const float midY = r.y + r.height * 0.5f;
    const float halfH = r.height * 0.5f - 3.0f;
    auto frameToX = [&](uint64_t frame) { return r.x + r.width * (float)((double)std::min<uint64_t>(frame, wave->frames) / (double)wave->frames); };

    bool regionMode = z.loop == Starsequence::LoopMode::Region;
    uint64_t loopA = 0, loopB = 0;
    bool showLoop = false;
    if (z.loop == Starsequence::LoopMode::Whole) { loopA = 0; loopB = wave->frames; showLoop = true; }
    else if (regionMode) { loopA = z.loopStart; loopB = std::min<uint64_t>(z.loopEnd, wave->frames); showLoop = loopA < loopB; }
    else if (z.loop == Starsequence::LoopMode::Wav && wave->hasLoop && wave->wavLoopStart < wave->wavLoopEnd) { loopA = wave->wavLoopStart; loopB = std::min<uint64_t>(wave->wavLoopEnd, wave->frames); showLoop = loopA < loopB; }
    if (showLoop) {
        float ax = frameToX(loopA), bx = frameToX(loopB);
        DrawRectangleRec({ ax, r.y + 1.0f, std::max(1.0f, bx - ax), r.height - 2.0f }, Fade(theme.accent, regionMode ? 0.22f : 0.14f));
    }

    DrawLine((int)r.x + 1, (int)midY, (int)(r.x + r.width) - 1, (int)midY, Fade(theme.textSecondary, 0.35f));
    const int buckets = (int)wave->peakMin.size();
    Color waveColor = ZoneColor(zoneIndex);
    int cols = std::max(1, (int)r.width - 2);
    for (int px = 0; px < cols; px++) {
        int b0 = px * buckets / cols;
        int b1 = std::max(b0 + 1, (px + 1) * buckets / cols);
        float lo = 0.0f, hi = 0.0f;
        for (int b = b0; b < b1 && b < buckets; b++) {
            lo = std::min(lo, wave->peakMin[(size_t)b]);
            hi = std::max(hi, wave->peakMax[(size_t)b]);
        }
        float x = r.x + 1.0f + (float)px;
        float y0 = midY - std::clamp(hi, -1.0f, 1.0f) * halfH;
        float y1 = midY - std::clamp(lo, -1.0f, 1.0f) * halfH;
        if (y1 - y0 < 1.0f) y1 = y0 + 1.0f;
        DrawLineV({ x, y0 }, { x, y1 }, waveColor);
    }

    if (regionMode) {
        Vector2 m = GetMousePosition();
        const std::string dragKey = id;
        auto xToFrame = [&](float mx) {
            double t = std::clamp((double)(mx - r.x) / (double)r.width, 0.0, 1.0);
            return (uint64_t)(t * (double)wave->frames);
        };

        if (loopDragId == dragKey) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                uint64_t f = xToFrame(m.x);
                if (loopDragWhich == 0) {
                    uint64_t end = z.loopEnd > 0 ? std::min<uint64_t>(z.loopEnd, wave->frames) : wave->frames;
                    f = std::min<uint64_t>(f, end > 1 ? end - 1 : 0);
                    if ((uint32_t)f != z.loopStart) { z.loopStart = (uint32_t)f; loopDragChanged = true; }
                } else {
                    f = std::max<uint64_t>(f, (uint64_t)z.loopStart + 1);
                    f = std::min<uint64_t>(f, wave->frames);
                    if ((uint32_t)f != z.loopEnd) { z.loopEnd = (uint32_t)f; loopDragChanged = true; }
                }
            } else {
                loopDragId.clear();
                if (loopDragChanged) { loopDragChanged = false; edited = true; }
            }
        } else if (loopDragId.empty() && Hot(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float ax = frameToX(z.loopStart);
            float bx = frameToX(z.loopEnd > 0 ? z.loopEnd : wave->frames);
            float da = std::fabs(m.x - ax), db = std::fabs(m.x - bx);
            if (std::min(da, db) <= 7.0f) {
                loopDragWhich = da <= db ? 0 : 1;
                if (z.loopEnd == 0) { z.loopEnd = (uint32_t)std::min<uint64_t>(wave->frames, 4294967000ull); loopDragChanged = true; }
                loopDragId = dragKey;
            } else if (session) {
                int key = std::clamp(z.rootKey, z.keyLow, z.keyHigh);
                session->BeginHeldAudition(instrument, key, std::clamp((z.velLow + z.velHigh) / 2, 1, 127), setIndex);
                loopPreviewHeld = true;
            }
        }

        float ax = frameToX(z.loopStart);
        float bx = frameToX(z.loopEnd > 0 ? z.loopEnd : wave->frames);
        bool hot = Hot(r) || loopDragId == dragKey;
        for (int which = 0; which < 2; which++) {
            float x = which == 0 ? ax : bx;
            bool active = loopDragId == dragKey && loopDragWhich == which;
            Color c = active ? WHITE : (hot ? theme.accent : Fade(theme.accent, 0.85f));
            DrawLineEx({ x, r.y + 1.0f }, { x, r.y + r.height - 1.0f }, 2.0f, c);
            float hx = which == 0 ? x : x - 7.0f;
            DrawRectangleRec({ hx, r.y + 1.0f, 7.0f, 9.0f }, c);
        }
    }
    return edited;
}

bool InstrumentEditorPanel::Hot(Rectangle r) const {
    if (picker.IsOpen()) return false;
    const void* scope = WidgetInput::GetInputScope();
    Vector2 m = GetMousePosition();
    if (WidgetInput::IsMouseBlocked()) return false;
    if (scope && WidgetInput::IsMouseOccluded(m, scope)) return false;
    return CheckCollisionPointRec(m, r) && CheckCollisionPointRec(m, bounds);
}

bool InstrumentEditorPanel::NumberField(const std::string& id, Rectangle r, float& value, int decimals, float lo, float hi, bool integer) {
    const UIPalette& theme = UITheme::Get();
    bool changed = false;
    bool hovered = Hot(r);
    DrawRectangleRec(r, theme.fieldBg);

    if (activeField == id) {
        int ch = GetCharPressed();
        while (ch > 0) {
            if (std::isdigit(ch) || ch == '.' || ch == '-' || ch == '+') {
                if (replaceOnType) { buffer.clear(); replaceOnType = false; }
                if (buffer.size() < 12) buffer.push_back((char)ch);
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (replaceOnType) { buffer.clear(); replaceOnType = false; }
            else if (!buffer.empty()) buffer.pop_back();
        }
        bool commit = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered);
        bool cancel = IsKeyPressed(KEY_ESCAPE);
        if (commit) {
            try {
                float parsed = std::stof(buffer);
                parsed = std::clamp(parsed, lo, hi);
                if (integer) parsed = (float)std::lround(parsed);
                if (parsed != value) { value = parsed; changed = true; }
            } catch (...) {}
            activeField.clear();
        } else if (cancel) {
            activeField.clear();
        }
        DrawRectangleLinesEx(r, 1, theme.accent);
        if (replaceOnType) DrawRectangle((int)r.x + 3, (int)r.y + 3, (int)std::min(r.width - 6.0f, TextWidth(buffer) + 4.0f), (int)r.height - 6, Fade(theme.accent, 0.35f));
        std::string shown = buffer + (std::fmod(GetTime(), 1.0) < 0.5 && !replaceOnType ? "|" : "");
        Text(shown, r.x + 5.0f, r.y + (r.height - kFontSize) / 2.0f, theme.textPrimary);
    } else {
        DrawRectangleLinesEx(r, 1, hovered ? theme.accent : theme.fieldBorder);
        std::string shown = FormatNumber(value, decimals, integer);
        Text(Fit(shown, r.width - 8.0f), r.x + 5.0f, r.y + (r.height - kFontSize) / 2.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            pendingField = id;
            pendingBuffer = shown;
        }
    }
    return changed;
}

bool InstrumentEditorPanel::TextField(const std::string& id, Rectangle r, std::string& value, size_t maxLength) {
    const UIPalette& theme = UITheme::Get();
    bool changed = false;
    bool hovered = Hot(r);
    DrawRectangleRec(r, theme.fieldBg);

    if (activeField == id) {
        int ch = GetCharPressed();
        while (ch > 0) {
            if (ch >= 32 && ch < 127) {
                if (replaceOnType) { buffer.clear(); replaceOnType = false; }
                if (buffer.size() < maxLength) buffer.push_back((char)ch);
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (replaceOnType) { buffer.clear(); replaceOnType = false; }
            else if (!buffer.empty()) buffer.pop_back();
        }
        bool commit = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered);
        bool cancel = IsKeyPressed(KEY_ESCAPE);
        if (commit) {
            if (!buffer.empty() && buffer != value) { value = buffer; changed = true; }
            activeField.clear();
        } else if (cancel) {
            activeField.clear();
        }
        DrawRectangleLinesEx(r, 1, theme.accent);
        if (replaceOnType) DrawRectangle((int)r.x + 3, (int)r.y + 3, (int)std::min(r.width - 6.0f, TextWidth(buffer) + 4.0f), (int)r.height - 6, Fade(theme.accent, 0.35f));
        std::string shown = buffer + (std::fmod(GetTime(), 1.0) < 0.5 && !replaceOnType ? "|" : "");
        Text(Fit(shown, r.width - 8.0f), r.x + 5.0f, r.y + (r.height - kFontSize) / 2.0f, theme.textPrimary);
    } else {
        DrawRectangleLinesEx(r, 1, hovered ? theme.accent : theme.fieldBorder);
        Text(Fit(value, r.width - 8.0f), r.x + 5.0f, r.y + (r.height - kFontSize) / 2.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            pendingField = id;
            pendingBuffer = value;
        }
    }
    return changed;
}

bool InstrumentEditorPanel::VSlider(const std::string& id, Rectangle r, float& value, float lo, float hi, float power, float resetValue) {
    const UIPalette& theme = UITheme::Get();
    bool changed = false;
    bool hovered = Hot(r);
    const float trackX = r.x + r.width * 0.5f - 3.0f;
    Rectangle track = { trackX, r.y + 4.0f, 6.0f, r.height - 8.0f };

    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        double now = GetTime();
        if (lastSliderId == id && now - lastSliderClick < 0.35) {
            if (value != resetValue) { value = resetValue; changed = true; }
            lastSliderClick = -10.0;
        } else {
            activeSlider = id;
            lastSliderId = id;
            lastSliderClick = now;
        }
    }
    if (activeSlider == id) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float t = 1.0f - std::clamp((GetMousePosition().y - track.y) / track.height, 0.0f, 1.0f);
            float mapped = lo + (hi - lo) * std::pow(t, power);
            if (mapped != value) { value = mapped; changed = true; }
        } else {
            activeSlider.clear();
        }
    }

    float norm = hi > lo ? std::clamp((value - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    float t = std::pow(norm, 1.0f / power);
    float thumbY = track.y + track.height * (1.0f - t);
    DrawRectangleRec(track, theme.fieldBg);
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    DrawRectangleRec({ track.x, thumbY, track.width, track.y + track.height - thumbY }, Fade(theme.accent, 0.75f));
    Rectangle thumb = { r.x + r.width * 0.5f - 9.0f, thumbY - 4.0f, 18.0f, 8.0f };
    DrawRectangleRec(thumb, activeSlider == id || hovered ? WHITE : theme.accent);
    DrawRectangleLinesEx(thumb, 1, Fade(BLACK, 0.5f));
    return changed;
}

void InstrumentEditorPanel::DrawEnvelope(Rectangle r, const SampleSet& set) const {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(r, ColorBrightness(theme.panelBg, -0.15f));
    DrawRectangleLinesEx(r, 1, theme.panelBorder);
    float attack = std::max(0.0f, set.attack), decay = std::max(0.0f, set.decay), release = std::max(0.0f, set.release);
    float sustain = std::clamp(set.sustain, 0.0f, 1.0f);
    float hold = std::max(0.15f, 0.25f * (attack + decay + release));
    float total = std::max(0.01f, attack + decay + hold + release);
    Rectangle plot = { r.x + 8.0f, r.y + 18.0f, r.width - 16.0f, r.height - 28.0f };
    auto X = [&](float t) { return plot.x + plot.width * (t / total); };
    auto Y = [&](float v) { return plot.y + plot.height * (1.0f - v); };
    Vector2 pts[5] = {
        { X(0.0f), Y(0.0f) }, { X(attack), Y(1.0f) }, { X(attack + decay), Y(sustain) },
        { X(attack + decay + hold), Y(sustain) }, { X(total), Y(0.0f) }
    };
    const float baseline = plot.y + plot.height;
    for (int i = 0; i < 4; i++) {
        Vector2 bottomA = { pts[i].x, baseline };
        Vector2 bottomB = { pts[i + 1].x, baseline };
        FillTriangle(pts[i], bottomA, bottomB, Fade(theme.accent, 0.2f));
        FillTriangle(pts[i], bottomB, pts[i + 1], Fade(theme.accent, 0.2f));
    }
    for (int i = 0; i < 4; i++) DrawLineEx(pts[i], pts[i + 1], 2.0f, theme.accent);
    Text("Envelope", r.x + 8.0f, r.y + 4.0f, theme.textSecondary, kSmallFontSize);
}

float InstrumentEditorPanel::DrawKeyMap(Rectangle r, SampleSet& set, int instrument, int setIndex) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(r, ColorBrightness(theme.panelBg, -0.15f));
    DrawRectangleLinesEx(r, 1, theme.panelBorder);
    const float keyWidth = r.width / 128.0f;
    for (int octave = 0; octave < 11; octave++) {
        float x = r.x + keyWidth * (float)(octave * 12);
        DrawLineEx({ x, r.y }, { x, r.y + r.height }, 1.0f, Fade(theme.textSecondary, 0.25f));
        Text("C" + std::to_string(octave - 1), x + 2.0f, r.y + r.height - 11.0f, theme.textSecondary, kSmallFontSize - 1.0f);
    }
    for (size_t i = 0; i < set.zones.size(); i++) {
        const SampleZone& z = set.zones[i];
        float x0 = r.x + keyWidth * (float)z.keyLow;
        float x1 = r.x + keyWidth * (float)(z.keyHigh + 1);
        Rectangle zr = { x0, r.y + 3.0f, std::max(2.0f, x1 - x0), r.height - 16.0f };
        bool selected = (int)i == selectedZone;
        DrawRectangleRec(zr, Fade(ZoneColor((int)i), selected ? 0.75f : 0.38f));
        if (selected) DrawRectangleLinesEx(zr, 2, WHITE);
        float rx = r.x + keyWidth * ((float)z.rootKey + 0.5f);
        DrawLineEx({ rx, zr.y }, { rx, zr.y + zr.height }, 1.0f, Fade(WHITE, 0.8f));
    }
    if (Hot(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int key = std::clamp((int)((GetMousePosition().x - r.x) / keyWidth), 0, 127);
        session->AuditionNote(instrument, key, session->defaultVelocity, setIndex);
    }
    return r.height;
}

void InstrumentEditorPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    Sequence& seq = session->GetSequence();

    DrawRectangleRec(bounds, theme.panelBg);
    ScissorStack::Push(bounds);
    pendingField.clear();
    wavesBuiltThisFrame = 0;
    if (loopPreviewHeld && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        session->EndHeldAudition();
        loopPreviewHeld = false;
    }

    const int cur = session->CurrentInstrument();
    if (cur < 0 || (size_t)cur >= seq.instruments.size()) {
        Text("Select an instrument to edit it.", bounds.x + kPad, bounds.y + kPad, theme.textSecondary);
        ScissorStack::Pop();
        return;
    }
    Instrument& inst = seq.instruments[(size_t)cur];
    int setIdx = std::clamp(session->SelectedSet(), 0, (int)inst.sets.size() - 1);
    if (setIdx != session->SelectedSet()) session->SetSelectedSet(setIdx);
    SampleSet& set = inst.sets[(size_t)setIdx];

    float maxScroll = std::max(0.0f, contentHeight - bounds.height);
    if (Hot(bounds) && activeSlider.empty()) scroll -= GetMouseWheelMove() * 40.0f;
    scroll = std::clamp(scroll, 0.0f, maxScroll);

    const float x = bounds.x + kPad;
    const float w = bounds.width - kPad * 2.0f - (maxScroll > 0.0f ? 6.0f : 0.0f);
    float y = bounds.y + kPad - scroll;
    bool edited = false;

    Text("Instrument", x, y + 6.0f, theme.textSecondary);
    std::string instName = inst.name;
    if (TextField("inst.name", { x + 72.0f, y, 230.0f, 24.0f }, instName, 40)) session->RenameInstrument(cur, instName);
    Text("#" + std::to_string(cur), x + 312.0f, y + 6.0f, theme.textSecondary);
    y += 34.0f;

    float tabX = x;
    for (size_t i = 0; i < inst.sets.size(); i++) {
        std::string label = Fit(inst.sets[i].name, 120.0f);
        float tabW = std::max(60.0f, TextWidth(label) + 20.0f);
        Rectangle tab = { tabX, y, tabW, 24.0f };
        bool isSelected = (int)i == setIdx;
        bool hovered = Hot(tab);
        DrawRectangleRec(tab, isSelected ? theme.surfaceSelected : (hovered ? theme.surfaceHover : theme.buttonBg));
        DrawRectangleLinesEx(tab, 1, isSelected ? theme.accent : theme.buttonBorder);
        Text(label, tab.x + 10.0f, tab.y + 6.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { session->SetSelectedSet((int)i); selectedZone = -1; }
        tabX += tabW + 4.0f;
    }
    {
        Rectangle add = { tabX, y, 28.0f, 24.0f };
        bool hovered = Hot(add);
        DrawRectangleRec(add, hovered ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(add, 1, theme.buttonBorder);
        Text("+", add.x + 10.0f, add.y + 6.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int added = AddSampleSet(inst);
            session->SetSelectedSet(added);
            session->MarkStructureChanged();
            selectedZone = -1;
            ScissorStack::Pop();
            return;
        }
    }
    y += 32.0f;

    Text("Set name", x, y + 6.0f, theme.textSecondary);
    std::string setName = set.name;
    if (TextField("set.name", { x + 72.0f, y, 200.0f, 24.0f }, setName, 40)) {
        bool taken = false;
        for (size_t i = 0; i < inst.sets.size(); i++) if ((int)i != setIdx && inst.sets[i].name == setName) taken = true;
        if (!taken) { set.name = setName; edited = true; }
    }
    if (inst.sets.size() > 1) {
        Rectangle remove = { x + 284.0f, y, 110.0f, 24.0f };
        bool hovered = Hot(remove);
        DrawRectangleRec(remove, hovered ? ColorBrightness(theme.danger, 0.1f) : Fade(theme.danger, 0.7f));
        DrawRectangleLinesEx(remove, 1, theme.buttonBorder);
        Text("Remove Set", remove.x + 18.0f, remove.y + 6.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            RemoveSampleSet(inst, setIdx);
            session->SetSelectedSet(0);
            session->MarkStructureChanged();
            selectedZone = -1;
            ScissorStack::Pop();
            return;
        }
    }
    y += 36.0f;

    const float sliderHeight = 118.0f;
    const float columnWidth = 58.0f;
    struct Column { const char* id; const char* label; float* value; float lo, hi, fieldHi, power, reset; int decimals; bool integer; };
    float volume = set.volume, pan = set.pan, bend = (float)set.bendRange;
    Column columns[7] = {
        { "volume", "Volume", &volume, 0.0f, 2.0f, 2.0f, 1.0f, 1.0f, 2, false },
        { "pan", "Pan", &pan, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 2, false },
        { "bend", "Bend", &bend, 0.0f, 24.0f, 24.0f, 1.0f, 2.0f, 0, true },
        { "attack", "Attack", &set.attack, 0.0f, 10.0f, 100.0f, 3.0f, 0.002f, 3, false },
        { "decay", "Decay", &set.decay, 0.0f, 20.0f, 100.0f, 3.0f, 0.1f, 3, false },
        { "sustain", "Sustain", &set.sustain, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 3, false },
        { "release", "Release", &set.release, 0.0f, 20.0f, 100.0f, 3.0f, 0.1f, 3, false },
    };
    for (int i = 0; i < 7; i++) {
        Column& c = columns[i];
        float cx = x + (float)i * columnWidth + (i >= 3 ? 14.0f : 0.0f);
        Text(c.label, cx + (columnWidth - TextWidth(c.label, kSmallFontSize)) * 0.5f, y, theme.textSecondary, kSmallFontSize);
        Rectangle sl = { cx, y + 16.0f, columnWidth - 8.0f, sliderHeight };
        if (VSlider(std::string("s.") + c.id, sl, *c.value, c.lo, c.hi, c.power, c.reset)) edited = true;
        Rectangle field = { cx, y + 16.0f + sliderHeight + 6.0f, columnWidth - 8.0f, 22.0f };
        if (NumberField(std::string("f.") + c.id, field, *c.value, c.decimals, c.lo, c.fieldHi, c.integer)) edited = true;
    }
    set.volume = volume;
    set.pan = pan;
    set.bendRange = (int)std::lround(bend);
    float envX = x + 7.0f * columnWidth + 14.0f + 16.0f;
    Rectangle envRect = { envX, y + 16.0f, std::max(120.0f, x + w - envX), sliderHeight + 28.0f };
    DrawEnvelope(envRect, set);
    y += 16.0f + sliderHeight + 40.0f;

    Text("Key map  (click to hear the selected set)", x, y, theme.textSecondary, kSmallFontSize);
    y += 14.0f;
    DrawKeyMap({ x, y, w, 46.0f }, set, cur, setIdx);
    y += 56.0f;

    Text("Samples", x, y + 5.0f, theme.textPrimary);
    Text(std::to_string(set.zones.size()), x + 60.0f, y + 5.0f, theme.textSecondary);
    {
        Rectangle add = { x + w - 110.0f, y, 110.0f, 24.0f };
        bool hovered = Hot(add);
        DrawRectangleRec(add, hovered ? ColorBrightness(theme.accent, 0.15f) : theme.accent);
        DrawRectangleLinesEx(add, 1, theme.buttonBorder);
        Text("+ Add Sample", add.x + 16.0f, add.y + 6.0f, theme.textPrimary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int added = AddZone(set, "");
            selectedZone = added;
            pickerZone = added;
            picker.Open(GetMousePosition(), ".starsound");
            session->MarkStructureChanged();
        }
    }
    y += 32.0f;

    int removeZone = -1;
    for (size_t zi = 0; zi < set.zones.size(); zi++) {
        SampleZone& z = set.zones[zi];
        const int idx = (int)zi;
        const float cardHeight = 168.0f;
        Rectangle card = { x, y, w, cardHeight };
        bool isSelected = idx == selectedZone;
        DrawRectangleRec(card, ColorBrightness(theme.panelBg, 0.06f));
        DrawRectangleLinesEx(card, isSelected ? 2.0f : 1.0f, isSelected ? theme.accent : theme.panelBorder);
        DrawRectangleRec({ card.x, card.y, 5.0f, card.height }, ZoneColor(idx));
        if (Hot(card) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedZone = idx;

        float cx = card.x + 14.0f;
        float cy = card.y + 8.0f;
        std::string stem = z.assetPath.empty() ? std::string("Choose sound...") : std::filesystem::path(z.assetPath).stem().string();
        Rectangle soundBtn = { cx, cy, std::max(150.0f, w - 14.0f - 170.0f), 24.0f };
        bool soundHover = Hot(soundBtn);
        DrawRectangleRec(soundBtn, soundHover ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(soundBtn, 1, z.assetPath.empty() ? theme.danger : theme.buttonBorder);
        Text(Fit(stem, soundBtn.width - 16.0f), soundBtn.x + 8.0f, soundBtn.y + 6.0f, theme.textPrimary);
        if (soundHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            pickerZone = idx;
            selectedZone = idx;
            picker.Open(GetMousePosition(), ".starsound");
        }
        Rectangle playBtn = { soundBtn.x + soundBtn.width + 8.0f, cy, 70.0f, 24.0f };
        bool playHover = Hot(playBtn);
        DrawRectangleRec(playBtn, playHover ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(playBtn, 1, theme.buttonBorder);
        Text("Play", playBtn.x + 22.0f, playBtn.y + 6.0f, theme.textPrimary);
        if (playHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int key = std::clamp(z.rootKey, z.keyLow, z.keyHigh);
            session->AuditionNote(cur, key, std::clamp((z.velLow + z.velHigh) / 2, 1, 127), setIdx);
        }
        Rectangle delBtn = { playBtn.x + playBtn.width + 8.0f, cy, 80.0f, 24.0f };
        bool delHover = Hot(delBtn);
        DrawRectangleRec(delBtn, delHover ? ColorBrightness(theme.danger, 0.1f) : Fade(theme.danger, 0.7f));
        DrawRectangleLinesEx(delBtn, 1, theme.buttonBorder);
        Text("Remove", delBtn.x + 18.0f, delBtn.y + 6.0f, theme.textPrimary);
        if (delHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) removeZone = idx;

        const std::string zid = "z" + std::to_string(idx) + ".";
        auto numberAt = [&](float fx, float fy, float fw, const char* label, const char* key, float value, float lo, float hi, int decimals, bool integer, auto&& apply) {
            Text(label, fx, fy, theme.textSecondary, kSmallFontSize);
            float v = value;
            if (NumberField(zid + key, { fx, fy + 12.0f, fw, 20.0f }, v, decimals, lo, hi, integer)) { apply(v); edited = true; }
        };
        float fy = cy + 30.0f;
        float fw = std::max(40.0f, (w - 28.0f - 7.0f * 6.0f) / 8.0f);
        float fx = cx;
        numberAt(fx, fy, fw, "Key Low", "kl", (float)z.keyLow, 0, 127, 0, true, [&](float v) { z.keyLow = (int)v; if (z.keyHigh < z.keyLow) z.keyHigh = z.keyLow; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Key High", "kh", (float)z.keyHigh, 0, 127, 0, true, [&](float v) { z.keyHigh = (int)v; if (z.keyLow > z.keyHigh) z.keyLow = z.keyHigh; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Root", "rk", (float)z.rootKey, 0, 127, 0, true, [&](float v) { z.rootKey = (int)v; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Vel Low", "vl", (float)z.velLow, 1, 127, 0, true, [&](float v) { z.velLow = (int)v; if (z.velHigh < z.velLow) z.velHigh = z.velLow; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Vel High", "vh", (float)z.velHigh, 1, 127, 0, true, [&](float v) { z.velHigh = (int)v; if (z.velLow > z.velHigh) z.velLow = z.velHigh; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Tune (cents)", "tu", z.tuneCents, -2400, 2400, 1, false, [&](float v) { z.tuneCents = v; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Gain (dB)", "ga", z.gainDb, -48, 24, 1, false, [&](float v) { z.gainDb = v; }); fx += fw + 6.0f;
        numberAt(fx, fy, fw, "Pan", "pn", z.pan, -1, 1, 2, false, [&](float v) { z.pan = v; });

        float ry = fy + 38.0f;
        Rectangle pitchBox = { cx, ry + 2.0f, 16.0f, 16.0f };
        bool pitchHover = Hot(pitchBox);
        DrawRectangleRec(pitchBox, theme.fieldBg);
        DrawRectangleLinesEx(pitchBox, 1, pitchHover ? theme.accent : theme.fieldBorder);
        if (z.pitched) DrawRectangleRec({ pitchBox.x + 4.0f, pitchBox.y + 4.0f, 8.0f, 8.0f }, theme.accent);
        Text("Follows pitch", cx + 22.0f, ry + 4.0f, theme.textPrimary);
        if (pitchHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { z.pitched = !z.pitched; edited = true; }

        Rectangle loopBtn = { cx + 130.0f, ry, 130.0f, 20.0f };
        bool loopHover = Hot(loopBtn);
        DrawRectangleRec(loopBtn, loopHover ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(loopBtn, 1, theme.buttonBorder);
        Text(LoopLabel(z.loop), loopBtn.x + 8.0f, loopBtn.y + 4.0f, theme.textPrimary);
        if (loopHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            z.loop = NextLoop(z.loop);
            if (z.loop == LoopMode::Region && z.loopEnd == 0) {
                const ZoneWave* seed = GetWave(z.assetPath);
                if (seed) {
                    bool useWav = seed->hasLoop && seed->wavLoopStart < seed->wavLoopEnd;
                    z.loopStart = useWav ? seed->wavLoopStart : 0;
                    z.loopEnd = useWav ? seed->wavLoopEnd : (uint32_t)std::min<uint64_t>(seed->frames, 4294967000ull);
                }
            }
            edited = true;
        }
        if (z.loop == LoopMode::Region) {
            float ls = (float)z.loopStart, le = (float)z.loopEnd;
            Text("Loop start", cx + 276.0f, ry + 4.0f, theme.textSecondary, kSmallFontSize);
            if (NumberField(zid + "ls", { cx + 336.0f, ry, 84.0f, 20.0f }, ls, 0, 0.0f, 4294967000.0f, true)) { z.loopStart = (uint32_t)ls; edited = true; }
            Text("end", cx + 430.0f, ry + 4.0f, theme.textSecondary, kSmallFontSize);
            if (NumberField(zid + "le", { cx + 456.0f, ry, 84.0f, 20.0f }, le, 0, 0.0f, 4294967000.0f, true)) { z.loopEnd = (uint32_t)le; edited = true; }
        }
        if (CheckCollisionRecs(card, bounds) || loopDragId == zid + "wave") {
            if (DrawZoneWave({ cx, ry + 28.0f, card.width - 28.0f, 52.0f }, z, idx, zid + "wave", cur, setIdx)) edited = true;
        }
        y += cardHeight + 8.0f;
    }
    if (removeZone >= 0) {
        RemoveZone(set, removeZone);
        selectedZone = -1;
        edited = true;
    }
    if (set.zones.empty()) Text("This sample set has no sounds yet. Add one, or import a SoundFont.", x, y + 4.0f, theme.textSecondary);
    y += 28.0f;

    if (edited) session->MarkStructureChanged();

    contentHeight = (y + scroll) - bounds.y + kPad;
    if (maxScroll > 0.0f) {
        float trackH = bounds.height - 4.0f;
        float thumbH = std::max(24.0f, trackH * bounds.height / contentHeight);
        float thumbY = bounds.y + 2.0f + (trackH - thumbH) * (scroll / maxScroll);
        DrawRectangleRec({ bounds.x + bounds.width - 6.0f, thumbY, 4.0f, thumbH }, Fade(theme.textSecondary, 0.5f));
    }

    ScissorStack::Pop();

    if (!pendingField.empty()) {
        activeField = pendingField;
        buffer = pendingBuffer;
        replaceOnType = true;
    }

    std::string picked;
    if (picker.Render(picked) && pickerZone >= 0 && (size_t)pickerZone < set.zones.size()) {
        set.zones[(size_t)pickerZone].assetPath = picked;
        waveCache.erase(picked);
        session->MarkStructureChanged();
    }
}
