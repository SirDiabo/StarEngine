#pragma once
#include "Modules/Starsequence/SequenceData.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Starsequence {

struct GridLine {
    int64_t tick = 0;
    int kind = 0;
    int bar = 0;
};

inline int AutoSubdivision(double pixelsPerBeat) {
    int best = 1;
    for (int s : { 2, 4, 8, 16 }) {
        if (pixelsPerBeat / (double)s >= 14.0) best = s;
    }
    return best;
}

inline std::vector<MeterPoint> EffectiveMeters(const Sequence& seq) {
    std::vector<MeterPoint> meters = seq.meter;
    if (meters.empty() || meters.front().tick != 0) meters.insert(meters.begin(), MeterPoint{ 0, 4, 4 });
    return meters;
}

inline void BuildGridLines(const Sequence& seq, double tickStart, double tickEnd, int subdivision, std::vector<GridLine>& out) {
    out.clear();
    const int resolution = std::max(1, seq.resolution);
    const std::vector<MeterPoint> meters = EffectiveMeters(seq);
    int64_t barsBefore = 0;
    for (size_t m = 0; m < meters.size(); m++) {
        const int numerator = std::max(1, meters[m].numerator);
        const int denominator = std::max(1, meters[m].denominator);
        const int64_t beatTicks = std::max<int64_t>(1, (int64_t)resolution * 4 / denominator);
        const int64_t barTicks = beatTicks * numerator;
        const int64_t segStart = meters[m].tick;
        const bool last = m + 1 == meters.size();
        const int64_t segEnd = last ? std::numeric_limits<int64_t>::max() : meters[m + 1].tick;
        if ((double)segStart > tickEnd) break;
        if (last || (double)segEnd >= tickStart) {
            int64_t first = 0;
            if (tickStart > (double)segStart) first = (int64_t)((tickStart - (double)segStart) / (double)beatTicks) - 1;
            if (first < 0) first = 0;
            for (int64_t i = first; out.size() < 40000; i++) {
                const int64_t t = segStart + i * beatTicks;
                if (t >= segEnd || (double)t > tickEnd) break;
                const bool isBar = (i % numerator) == 0;
                const int barNumber = (int)(barsBefore + i / numerator + 1);
                out.push_back({ t, isBar ? 2 : 1, barNumber });
                for (int j = 1; j < subdivision; j++) {
                    const int64_t st = t + beatTicks * j / subdivision;
                    if (st >= segEnd || (double)st > tickEnd) break;
                    out.push_back({ st, 0, barNumber });
                }
            }
        }
        if (last) break;
        barsBefore += (segEnd - segStart + barTicks - 1) / barTicks;
    }
}

inline void TickToBarBeat(const Sequence& seq, int64_t tick, int& bar, int& beat) {
    const int resolution = std::max(1, seq.resolution);
    const std::vector<MeterPoint> meters = EffectiveMeters(seq);
    int64_t barsBefore = 0;
    bar = 1;
    beat = 1;
    for (size_t m = 0; m < meters.size(); m++) {
        const int numerator = std::max(1, meters[m].numerator);
        const int denominator = std::max(1, meters[m].denominator);
        const int64_t beatTicks = std::max<int64_t>(1, (int64_t)resolution * 4 / denominator);
        const int64_t barTicks = beatTicks * numerator;
        const int64_t segStart = meters[m].tick;
        const bool last = m + 1 == meters.size();
        const int64_t segEnd = last ? std::numeric_limits<int64_t>::max() : meters[m + 1].tick;
        if (last || tick < segEnd) {
            const int64_t rel = std::max<int64_t>(0, tick - segStart);
            bar = (int)(barsBefore + rel / barTicks + 1);
            beat = (int)((rel % barTicks) / beatTicks + 1);
            return;
        }
        barsBefore += (segEnd - segStart + barTicks - 1) / barTicks;
    }
}

inline const char* KeyName(int key, int& octave) {
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int k = std::clamp(key, 0, 127);
    octave = k / 12 - 1;
    return names[k % 12];
}

inline bool IsBlackKey(int key) {
    int n = ((key % 12) + 12) % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

struct PianoRollView {
    static constexpr double kMinPixelsPerBeat = 6.0;
    static constexpr double kMaxPixelsPerBeat = 1200.0;
    static constexpr float kMinKeyHeight = 6.0f;
    static constexpr float kMaxKeyHeight = 32.0f;

    double scrollTick = 0.0;
    double pixelsPerBeat = 80.0;
    double topKey = 90.0;
    float keyHeight = 14.0f;
    int resolution = 480;

    double PixelsPerTick() const { return pixelsPerBeat / (double)std::max(1, resolution); }
    float XOfTick(double tick, float originX) const { return originX + (float)((tick - scrollTick) * PixelsPerTick()); }
    double TickOfX(float x, float originX) const { return scrollTick + (double)(x - originX) / PixelsPerTick(); }
    float YOfKey(int key, float originY) const { return originY + (float)((topKey - (double)key) * (double)keyHeight); }
    int KeyOfY(float y, float originY) const {
        double t = topKey - (double)(y - originY) / (double)keyHeight;
        return (int)std::ceil(t);
    }

    void ClampHorizontal() {
        if (scrollTick < 0.0) scrollTick = 0.0;
    }

    void ClampVertical(float viewHeight) {
        double visible = (double)viewHeight / (double)keyHeight;
        double lo = visible - 1.0;
        double hi = 127.0;
        if (lo > hi) topKey = hi;
        else topKey = std::clamp(topKey, lo, hi);
    }

    void ZoomTime(double factor, float anchorX, float originX) {
        double anchorTick = TickOfX(anchorX, originX);
        pixelsPerBeat = std::clamp(pixelsPerBeat * factor, kMinPixelsPerBeat, kMaxPixelsPerBeat);
        scrollTick = anchorTick - (double)(anchorX - originX) / PixelsPerTick();
        ClampHorizontal();
    }

    void ZoomKeys(double factor, float anchorY, float originY, float viewHeight) {
        double anchorKey = topKey - (double)(anchorY - originY) / (double)keyHeight;
        keyHeight = std::clamp((float)((double)keyHeight * factor), kMinKeyHeight, kMaxKeyHeight);
        topKey = anchorKey + (double)(anchorY - originY) / (double)keyHeight;
        ClampVertical(viewHeight);
    }

    void ScrollTicks(double deltaTicks) {
        scrollTick += deltaTicks;
        ClampHorizontal();
    }

    void ScrollKeys(double deltaKeys, float viewHeight) {
        topKey += deltaKeys;
        ClampVertical(viewHeight);
    }
};

}
