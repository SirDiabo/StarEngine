#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Starsequence {
namespace Midi {

enum class EventType { NoteOff, NoteOn, ControlChange, ProgramChange, PitchBend, Tempo, TimeSignature, Marker, TrackName };

struct Event {
    int64_t tick = 0;
    int track = 0;
    uint32_t order = 0;
    EventType type = EventType::NoteOn;
    int channel = 0;
    int a = 0;
    int b = 0;
    std::string text;
};

struct TrackInfo {
    std::string name;
    int64_t endTick = 0;
    int eventCount = 0;
};

struct File {
    int format = 1;
    int declaredTracks = 0;
    bool smpte = false;
    int ppq = 480;
    double smpteTicksPerSecond = 0.0;
    std::vector<TrackInfo> tracks;
    std::vector<Event> events;
};

struct Diagnostic {
    bool error = false;
    std::string text;
};

bool ParseMidi(const std::vector<uint8_t>& data, File& out, std::vector<Diagnostic>& diagnostics);
std::string SanitizeText(const std::string& raw);

}
}
