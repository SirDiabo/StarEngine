#include "MidiFile.h"
#include <algorithm>

namespace Starsequence {
namespace Midi {

namespace {

constexpr int64_t kMaxTick = (int64_t)1 << 40;
constexpr size_t kSearchLimit = 4096;

struct Reader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool Has(size_t count) const { return pos <= size && size - pos >= count; }
    int Byte() { return data[pos++]; }

    bool ReadVlq(uint32_t& value) {
        value = 0;
        for (int i = 0; i < 4; i++) {
            if (pos >= size) return false;
            uint8_t b = data[pos++];
            value = (value << 7) | (b & 0x7F);
            if (!(b & 0x80)) return true;
        }
        return false;
    }
};

uint32_t ReadBe32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint32_t ReadLe32(const uint8_t* p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

int ReadBe16(const uint8_t* p) {
    return ((int)p[0] << 8) | (int)p[1];
}

bool Tag(const uint8_t* p, const char* tag) {
    return p[0] == (uint8_t)tag[0] && p[1] == (uint8_t)tag[1] && p[2] == (uint8_t)tag[2] && p[3] == (uint8_t)tag[3];
}

void AddDiagnostic(std::vector<Diagnostic>& diagnostics, bool error, const std::string& text) {
    diagnostics.push_back({ error, text });
}

int ChannelDataBytes(int status) {
    int type = status & 0xF0;
    return (type == 0xC0 || type == 0xD0) ? 1 : 2;
}

void ParseTrack(const uint8_t* data, size_t size, int trackIndex, File& file, std::vector<Diagnostic>& diagnostics, uint32_t& order) {
    Reader r;
    r.data = data;
    r.size = size;
    TrackInfo info;
    int64_t tick = 0;
    int runningStatus = 0;
    bool ended = false;
    bool damaged = false;

    auto emit = [&](Event e) {
        e.track = trackIndex;
        e.tick = tick;
        e.order = order++;
        file.events.push_back(std::move(e));
        info.eventCount++;
    };

    while (r.pos < r.size && !ended) {
        uint32_t delta = 0;
        if (!r.ReadVlq(delta)) { damaged = true; break; }
        tick += delta;
        if (tick > kMaxTick) { damaged = true; break; }
        if (!r.Has(1)) { damaged = true; break; }

        int status = data[r.pos];
        if (status < 0x80) {
            if (runningStatus == 0) { damaged = true; break; }
            status = runningStatus;
        } else {
            r.pos++;
        }

        if (status == 0xFF) {
            runningStatus = 0;
            if (!r.Has(1)) { damaged = true; break; }
            int type = r.Byte();
            uint32_t length = 0;
            if (!r.ReadVlq(length) || !r.Has(length)) { damaged = true; break; }
            const uint8_t* payload = data + r.pos;
            r.pos += length;
            if (type == 0x2F) {
                ended = true;
            } else if (type == 0x51 && length >= 3) {
                Event e;
                e.type = EventType::Tempo;
                e.a = (int)(((uint32_t)payload[0] << 16) | ((uint32_t)payload[1] << 8) | payload[2]);
                emit(e);
            } else if (type == 0x58 && length >= 2) {
                Event e;
                e.type = EventType::TimeSignature;
                int power = std::min<int>(payload[1], 7);
                e.a = payload[0];
                e.b = 1 << power;
                emit(e);
            } else if (type == 0x03) {
                if (info.name.empty()) info.name = SanitizeText(std::string((const char*)payload, length));
            } else if (type == 0x06 || type == 0x07) {
                Event e;
                e.type = EventType::Marker;
                e.text = SanitizeText(std::string((const char*)payload, length));
                if (!e.text.empty()) emit(e);
            }
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            runningStatus = 0;
            uint32_t length = 0;
            if (!r.ReadVlq(length) || !r.Has(length)) { damaged = true; break; }
            r.pos += length;
            continue;
        }

        if (status >= 0xF1) {
            runningStatus = 0;
            size_t skip = status == 0xF2 ? 2 : ((status == 0xF1 || status == 0xF3) ? 1 : 0);
            if (!r.Has(skip)) { damaged = true; break; }
            r.pos += skip;
            continue;
        }

        runningStatus = status;
        int count = ChannelDataBytes(status);
        if (!r.Has((size_t)count)) { damaged = true; break; }
        int d1 = data[r.pos];
        int d2 = count == 2 ? data[r.pos + 1] : 0;
        if ((d1 & 0x80) || (count == 2 && (d2 & 0x80))) { damaged = true; break; }
        r.pos += (size_t)count;

        Event e;
        e.channel = status & 0x0F;
        int type = status & 0xF0;
        if (type == 0x80) {
            e.type = EventType::NoteOff; e.a = d1; e.b = d2; emit(e);
        } else if (type == 0x90) {
            e.type = d2 == 0 ? EventType::NoteOff : EventType::NoteOn; e.a = d1; e.b = d2; emit(e);
        } else if (type == 0xB0) {
            e.type = EventType::ControlChange; e.a = d1; e.b = d2; emit(e);
        } else if (type == 0xC0) {
            e.type = EventType::ProgramChange; e.a = d1; emit(e);
        } else if (type == 0xE0) {
            e.type = EventType::PitchBend; e.a = d1 | (d2 << 7); emit(e);
        }
    }

    info.endTick = tick;
    file.tracks.push_back(info);
    if (damaged) {
        std::string name = info.name.empty() ? std::to_string(trackIndex + 1) : std::to_string(trackIndex + 1) + " (" + info.name + ")";
        AddDiagnostic(diagnostics, false, "Track " + name + " is damaged or cut off; only the events before the damage were imported");
    }
}

}

std::string SanitizeText(const std::string& raw) {
    std::string result;
    for (unsigned char c : raw) {
        if (c == 0) break;
        if (c < 0x20 || c == 0x7F) continue;
        result.push_back(c >= 0x80 ? '?' : (char)c);
    }
    size_t start = result.find_first_not_of(' ');
    if (start == std::string::npos) return std::string();
    size_t end = result.find_last_not_of(' ');
    return result.substr(start, end - start + 1);
}

bool ParseMidi(const std::vector<uint8_t>& input, File& out, std::vector<Diagnostic>& diagnostics) {
    out = File();
    const uint8_t* base = input.data();
    size_t size = input.size();

    if (size >= 20 && Tag(base, "RIFF") && Tag(base + 8, "RMID")) {
        size_t pos = 12;
        bool found = false;
        while (pos + 8 <= size) {
            uint32_t chunkSize = ReadLe32(base + pos + 4);
            if (Tag(base + pos, "data")) {
                size_t available = size - (pos + 8);
                base = base + pos + 8;
                size = std::min<size_t>(chunkSize, available);
                found = true;
                break;
            }
            size_t next = pos + 8 + (size_t)chunkSize + (chunkSize & 1);
            if (next <= pos) break;
            pos = next;
        }
        if (!found) {
            AddDiagnostic(diagnostics, true, "The RIFF MIDI file has no data chunk");
            return false;
        }
    }

    size_t start = std::string::npos;
    size_t limit = std::min(size, kSearchLimit);
    for (size_t i = 0; i + 4 <= limit; i++) {
        if (Tag(base + i, "MThd")) { start = i; break; }
    }
    if (start == std::string::npos) {
        AddDiagnostic(diagnostics, true, "This is not a standard MIDI file (no MThd header found)");
        return false;
    }
    base += start;
    size -= start;

    if (size < 14) {
        AddDiagnostic(diagnostics, true, "The MIDI header is cut off");
        return false;
    }
    uint32_t headerLength = ReadBe32(base + 4);
    if (headerLength < 6 || headerLength > size - 8) {
        AddDiagnostic(diagnostics, true, "The MIDI header has an invalid length");
        return false;
    }
    out.format = ReadBe16(base + 8);
    out.declaredTracks = ReadBe16(base + 10);
    int division = ReadBe16(base + 12);
    if (division == 0) {
        AddDiagnostic(diagnostics, true, "The MIDI file has an invalid time division");
        return false;
    }
    if (division & 0x8000) {
        int fps = 256 - ((division >> 8) & 0xFF);
        int perFrame = division & 0xFF;
        double framesPerSecond = fps == 29 ? 29.97 : (double)fps;
        if (perFrame == 0 || !(fps == 24 || fps == 25 || fps == 29 || fps == 30)) {
            AddDiagnostic(diagnostics, true, "The MIDI file has an invalid SMPTE time division");
            return false;
        }
        out.smpte = true;
        out.smpteTicksPerSecond = framesPerSecond * (double)perFrame;
    } else {
        out.ppq = division;
    }
    if (out.format > 2) {
        AddDiagnostic(diagnostics, false, "The MIDI file reports an unknown format (" + std::to_string(out.format) + "); it was read as format 1");
    } else if (out.format == 2) {
        AddDiagnostic(diagnostics, false, "Format 2 MIDI files contain independent songs; all of them were placed on one shared timeline");
    }

    size_t pos = 8 + headerLength;
    uint32_t order = 0;
    int trackIndex = 0;
    while (pos + 8 <= size) {
        uint32_t chunkLength = ReadBe32(base + pos + 4);
        size_t dataStart = pos + 8;
        size_t available = size - dataStart;
        size_t length = std::min<size_t>(chunkLength, available);
        if (Tag(base + pos, "MTrk")) {
            if (chunkLength > available) {
                AddDiagnostic(diagnostics, false, "Track " + std::to_string(trackIndex + 1) + " is cut off at the end of the file");
            }
            ParseTrack(base + dataStart, length, trackIndex, out, diagnostics, order);
            trackIndex++;
        }
        if (chunkLength > available) break;
        pos = dataStart + length;
    }

    if (trackIndex == 0) {
        AddDiagnostic(diagnostics, true, "The MIDI file contains no tracks");
        return false;
    }
    if (out.declaredTracks > trackIndex) {
        AddDiagnostic(diagnostics, false, "The MIDI header declares " + std::to_string(out.declaredTracks) + " tracks but only " + std::to_string(trackIndex) + " could be read");
    }
    return true;
}

}
}
