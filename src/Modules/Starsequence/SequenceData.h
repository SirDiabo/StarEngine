#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Starsequence {

constexpr int kFormatVersion = 1;
constexpr int kMaxInstruments = 64;
constexpr int kMaxLayers = 64;
constexpr int kMaxPolyphony = 128;

enum class LoopMode { None, Whole, Region, Wav };

struct SampleZone {
    std::string assetPath;
    int keyLow = 0;
    int keyHigh = 127;
    int velLow = 1;
    int velHigh = 127;
    int rootKey = 60;
    LoopMode loop = LoopMode::None;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    bool pitched = true;
    float tuneCents = 0.0f;
    float gainDb = 0.0f;
    float pan = 0.0f;
};

struct SampleSet {
    std::string name = "Default";
    std::vector<SampleZone> zones;
    float volume = 1.0f;
    float pan = 0.0f;
    int bendRange = 2;
    float attack = 0.002f;
    float decay = 0.1f;
    float sustain = 1.0f;
    float release = 0.1f;
};

struct Instrument {
    std::string name;
    std::vector<SampleSet> sets;
};

struct Note {
    int instrument = 0;
    int64_t tick = 0;
    int64_t length = 0;
    int key = 60;
    int velocity = 100;
};

enum class ControllerType { Volume, Expression, Pan, Bend, Sustain, ReverbSend, ChorusSend };

struct ControllerEvent {
    ControllerType type = ControllerType::Volume;
    int instrument = 0;
    int64_t tick = 0;
    int value = 0;
};

struct Layer {
    std::string name;
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    std::vector<Note> notes;
    std::vector<ControllerEvent> controllers;
};

struct TempoPoint {
    int64_t tick = 0;
    double bpm = 120.0;
};

struct MeterPoint {
    int64_t tick = 0;
    int numerator = 4;
    int denominator = 4;
};

struct Marker {
    int64_t tick = 0;
    std::string name;
};

struct Sequence {
    std::string name;
    std::string bus = "Music";
    int resolution = 480;
    int64_t length = 0;
    int64_t loopStart = 0;
    int64_t loopEnd = 0;
    int polyphony = 64;
    float layerFade = 0.2f;
    float volume = 1.0f;

    std::vector<TempoPoint> tempo;
    std::vector<MeterPoint> meter;
    std::vector<Marker> markers;
    std::vector<Instrument> instruments;
    std::vector<Layer> layers;

    double BpmAt(int64_t tick) const;
    double TickToSeconds(double tick) const;
    double SecondsToTick(double seconds) const;
    double DurationSeconds() const { return TickToSeconds((double)length); }
    int FindInstrument(const std::string& instrumentName) const;
    int FindLayer(const std::string& layerName) const;
    int FindMarker(const std::string& markerName) const;
    int64_t BeatTicksAt(int64_t tick) const;
    int64_t BarTicksAt(int64_t tick) const;
    void BuildBoundaries(std::vector<int64_t>& beats, std::vector<int64_t>& bars) const;
    int64_t ComputeContentEnd() const;
    void Normalize();
};

}
