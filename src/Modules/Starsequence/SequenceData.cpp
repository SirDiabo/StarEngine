#include "SequenceData.h"
#include <algorithm>
#include <cmath>

namespace Starsequence {

double Sequence::BpmAt(int64_t tick) const {
    double bpm = 120.0;
    for (const TempoPoint& p : tempo) {
        if (p.tick > tick) break;
        bpm = p.bpm;
    }
    return bpm;
}

double Sequence::TickToSeconds(double tick) const {
    if (resolution <= 0) return 0.0;
    double seconds = 0.0;
    double cursor = 0.0;
    double bpm = tempo.empty() ? 120.0 : tempo.front().bpm;
    for (size_t i = 0; i < tempo.size(); i++) {
        double pointTick = (double)tempo[i].tick;
        if (pointTick >= tick) break;
        if (pointTick > cursor) {
            seconds += (pointTick - cursor) * 60.0 / (bpm * (double)resolution);
            cursor = pointTick;
        }
        bpm = tempo[i].bpm;
    }
    seconds += (tick - cursor) * 60.0 / (bpm * (double)resolution);
    return seconds;
}

double Sequence::SecondsToTick(double seconds) const {
    if (resolution <= 0) return 0.0;
    double remaining = seconds;
    double cursor = 0.0;
    double bpm = tempo.empty() ? 120.0 : tempo.front().bpm;
    for (size_t i = 0; i < tempo.size(); i++) {
        double pointTick = (double)tempo[i].tick;
        if (pointTick > cursor) {
            double segment = (pointTick - cursor) * 60.0 / (bpm * (double)resolution);
            if (segment >= remaining) break;
            remaining -= segment;
            cursor = pointTick;
        }
        bpm = tempo[i].bpm;
    }
    return cursor + remaining * bpm * (double)resolution / 60.0;
}

int Sequence::FindInstrument(const std::string& instrumentName) const {
    for (size_t i = 0; i < instruments.size(); i++) if (instruments[i].name == instrumentName) return (int)i;
    return -1;
}

int Sequence::FindLayer(const std::string& layerName) const {
    for (size_t i = 0; i < layers.size(); i++) if (layers[i].name == layerName) return (int)i;
    return -1;
}

int Sequence::FindMarker(const std::string& markerName) const {
    for (size_t i = 0; i < markers.size(); i++) if (markers[i].name == markerName) return (int)i;
    return -1;
}

int64_t Sequence::BeatTicksAt(int64_t tick) const {
    int denominator = 4;
    for (const MeterPoint& m : meter) {
        if (m.tick > tick) break;
        denominator = m.denominator;
    }
    if (denominator <= 0) denominator = 4;
    return std::max<int64_t>(1, (int64_t)resolution * 4 / denominator);
}

int64_t Sequence::BarTicksAt(int64_t tick) const {
    int numerator = 4;
    for (const MeterPoint& m : meter) {
        if (m.tick > tick) break;
        numerator = m.numerator;
    }
    return std::max<int64_t>(1, BeatTicksAt(tick) * std::max(1, numerator));
}

void Sequence::BuildBoundaries(std::vector<int64_t>& beats, std::vector<int64_t>& bars) const {
    beats.clear();
    bars.clear();
    int64_t end = std::max<int64_t>(length, 1);
    int64_t tick = 0;
    size_t guard = 0;
    while (tick <= end && guard++ < 2000000) {
        bars.push_back(tick);
        int64_t beat = BeatTicksAt(tick);
        int64_t barLength = BarTicksAt(tick);
        for (int64_t b = tick; b < tick + barLength && b <= end; b += beat) beats.push_back(b);
        tick += barLength;
    }
}

int64_t Sequence::ComputeContentEnd() const {
    int64_t end = 0;
    for (const Layer& layer : layers) {
        for (const Note& n : layer.notes) end = std::max(end, n.tick + n.length);
        for (const ControllerEvent& c : layer.controllers) end = std::max(end, c.tick);
    }
    return end;
}

void Sequence::Normalize() {
    if (resolution <= 0) resolution = 480;
    if (tempo.empty() || tempo.front().tick != 0) tempo.insert(tempo.begin(), TempoPoint{ 0, tempo.empty() ? 120.0 : tempo.front().bpm });
    if (meter.empty() || meter.front().tick != 0) meter.insert(meter.begin(), MeterPoint{ 0, 4, 4 });
    std::stable_sort(tempo.begin(), tempo.end(), [](const TempoPoint& a, const TempoPoint& b) { return a.tick < b.tick; });
    std::stable_sort(meter.begin(), meter.end(), [](const MeterPoint& a, const MeterPoint& b) { return a.tick < b.tick; });
    std::stable_sort(markers.begin(), markers.end(), [](const Marker& a, const Marker& b) { return a.tick < b.tick; });
    for (Layer& layer : layers) {
        std::stable_sort(layer.notes.begin(), layer.notes.end(), [](const Note& a, const Note& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.instrument != b.instrument) return a.instrument < b.instrument;
            return a.key < b.key;
        });
        std::stable_sort(layer.controllers.begin(), layer.controllers.end(), [](const ControllerEvent& a, const ControllerEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.instrument != b.instrument) return a.instrument < b.instrument;
            return (int)a.type < (int)b.type;
        });
    }
    if (length <= 0) {
        int64_t content = ComputeContentEnd();
        int64_t bar = BarTicksAt(0);
        length = ((content + bar - 1) / bar) * bar;
        if (length <= 0) length = bar;
    }
    if (loopEnd > length) loopEnd = length;
    if (loopStart < 0 || loopStart >= loopEnd) loopStart = 0;
    polyphony = std::clamp(polyphony, 1, kMaxPolyphony);
    layerFade = std::clamp(layerFade, 0.0f, 30.0f);
}

}
