#include "SequenceFormat.h"
#include "Core/CookedFile.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Starsequence {

namespace {

std::string Trim(const std::string& text) {
    size_t a = text.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = text.find_last_not_of(" \t\r\n");
    return text.substr(a, b - a + 1);
}

std::vector<std::string> Split(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string token;
    while (in >> token) out.push_back(token);
    return out;
}

bool ParseInt(const std::string& text, int64_t& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    long long v = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return false;
    out = (int64_t)v;
    return true;
}

bool ParseDouble(const std::string& text, double& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool ParseBool(const std::string& text, bool& out) {
    if (text == "true" || text == "1") { out = true; return true; }
    if (text == "false" || text == "0") { out = false; return true; }
    return false;
}

bool ParseRange(const std::string& text, int& low, int& high) {
    size_t dash = text.find('-', 1);
    int64_t a = 0, b = 0;
    if (dash == std::string::npos) {
        if (!ParseInt(text, a)) return false;
        low = high = (int)a;
        return true;
    }
    if (!ParseInt(text.substr(0, dash), a) || !ParseInt(text.substr(dash + 1), b)) return false;
    low = (int)a;
    high = (int)b;
    return true;
}

std::string FormatNumber(double value) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%.6g", value);
    return buf;
}

bool IsSampleProperty(const std::string& token) {
    size_t eq = token.find('=');
    if (eq == std::string::npos) return false;
    std::string key = token.substr(0, eq);
    return key == "keys" || key == "vel" || key == "root" || key == "loop" || key == "pitched" || key == "tune" || key == "gain" || key == "pan";
}

constexpr int64_t kMaxTick = (int64_t)1 << 40;

enum class Section { Header, Tempo, Meter, Markers, Instrument, Layer, Unknown };

struct Parser {
    ParseResult result;
    Section section = Section::Header;
    int line = 0;
    int currentInstrument = -1;
    int currentLayer = -1;

    void Error(const std::string& message) { result.diagnostics.push_back({ line, true, message }); }
    void Warn(const std::string& message) { result.diagnostics.push_back({ line, false, message }); }

    SampleSet& CurrentSet() {
        Instrument& inst = result.sequence.instruments[(size_t)currentInstrument];
        if (inst.sets.empty()) inst.sets.push_back(SampleSet());
        return inst.sets.back();
    }

    void ParseSampleLine(const std::string& value) {
        std::vector<std::string> tokens = Split(value);
        size_t first = 0;
        while (first < tokens.size() && !IsSampleProperty(tokens[first])) first++;
        if (first == 0) { Error("Sample line has no asset path"); return; }

        SampleZone zone;
        for (size_t i = 0; i < first; i++) zone.assetPath += (i ? " " : "") + tokens[i];

        for (size_t i = first; i < tokens.size(); i++) {
            size_t eq = tokens[i].find('=');
            std::string key = tokens[i].substr(0, eq);
            std::string val = tokens[i].substr(eq + 1);
            double number = 0.0;
            bool flag = false;
            if (key == "keys") {
                if (!ParseRange(val, zone.keyLow, zone.keyHigh)) Error("Invalid keys range '" + val + "'");
            } else if (key == "vel") {
                if (!ParseRange(val, zone.velLow, zone.velHigh)) Error("Invalid vel range '" + val + "'");
            } else if (key == "root") {
                int64_t v = 0;
                if (ParseInt(val, v)) zone.rootKey = (int)v; else Error("Invalid root key '" + val + "'");
            } else if (key == "loop") {
                if (val == "none") zone.loop = LoopMode::None;
                else if (val == "whole") zone.loop = LoopMode::Whole;
                else if (val == "wav") zone.loop = LoopMode::Wav;
                else if (val.rfind("region:", 0) == 0) {
                    int a = 0, b = 0;
                    if (ParseRange(val.substr(7), a, b) && a >= 0 && b > a) {
                        zone.loop = LoopMode::Region;
                        zone.loopStart = (uint32_t)a;
                        zone.loopEnd = (uint32_t)b;
                    } else {
                        Error("Invalid loop region '" + val + "'");
                    }
                } else {
                    Error("Unknown loop mode '" + val + "'");
                }
            } else if (key == "pitched") {
                if (ParseBool(val, flag)) zone.pitched = flag; else Error("Invalid pitched value '" + val + "'");
            } else if (key == "tune") {
                if (ParseDouble(val, number)) zone.tuneCents = (float)number; else Error("Invalid tune '" + val + "'");
            } else if (key == "gain") {
                if (ParseDouble(val, number)) zone.gainDb = (float)number; else Error("Invalid gain '" + val + "'");
            } else if (key == "pan") {
                if (ParseDouble(val, number)) zone.pan = std::clamp((float)number, -1.0f, 1.0f); else Error("Invalid pan '" + val + "'");
            }
        }

        zone.keyLow = std::clamp(zone.keyLow, 0, 127);
        zone.keyHigh = std::clamp(zone.keyHigh, 0, 127);
        zone.velLow = std::clamp(zone.velLow, 1, 127);
        zone.velHigh = std::clamp(zone.velHigh, 1, 127);
        if (zone.keyLow > zone.keyHigh) std::swap(zone.keyLow, zone.keyHigh);
        if (zone.velLow > zone.velHigh) std::swap(zone.velLow, zone.velHigh);
        CurrentSet().zones.push_back(zone);
    }

    void ParseInstrumentKey(const std::string& key, const std::string& value) {
        Instrument& inst = result.sequence.instruments[(size_t)currentInstrument];
        double number = 0.0;
        int64_t integer = 0;
        if (key == "Name") {
            inst.name = value;
        } else if (key == "Set") {
            SampleSet set;
            set.name = value.empty() ? "Set" : value;
            inst.sets.push_back(set);
        } else if (key == "Sample") {
            ParseSampleLine(value);
        } else if (key == "Volume") {
            if (ParseDouble(value, number)) CurrentSet().volume = std::max(0.0f, (float)number); else Error("Invalid Volume");
        } else if (key == "Pan") {
            if (ParseDouble(value, number)) CurrentSet().pan = std::clamp((float)number, -1.0f, 1.0f); else Error("Invalid Pan");
        } else if (key == "BendRange") {
            if (ParseInt(value, integer)) CurrentSet().bendRange = (int)std::clamp<int64_t>(integer, 0, 48); else Error("Invalid BendRange");
        } else if (key == "Attack") {
            if (ParseDouble(value, number)) CurrentSet().attack = std::max(0.0f, (float)number); else Error("Invalid Attack");
        } else if (key == "Decay") {
            if (ParseDouble(value, number)) CurrentSet().decay = std::max(0.0f, (float)number); else Error("Invalid Decay");
        } else if (key == "Sustain") {
            if (ParseDouble(value, number)) CurrentSet().sustain = std::clamp((float)number, 0.0f, 1.0f); else Error("Invalid Sustain");
        } else if (key == "Release") {
            if (ParseDouble(value, number)) CurrentSet().release = std::max(0.0f, (float)number); else Error("Invalid Release");
        } else {
            Warn("Unknown instrument key '" + key + "'");
        }
    }

    void ParseHeaderKey(const std::string& key, const std::string& value) {
        Sequence& s = result.sequence;
        double number = 0.0;
        int64_t integer = 0;
        if (key == "Name") s.name = value;
        else if (key == "Bus") s.bus = value;
        else if (key == "Resolution") { if (ParseInt(value, integer) && integer > 0 && integer <= 100000) s.resolution = (int)integer; else Error("Invalid Resolution"); }
        else if (key == "Length") { if (ParseInt(value, integer) && integer >= 0 && integer <= kMaxTick) s.length = integer; else Error("Invalid Length"); }
        else if (key == "LoopStart") { if (ParseInt(value, integer) && integer >= 0 && integer <= kMaxTick) s.loopStart = integer; else Error("Invalid LoopStart"); }
        else if (key == "LoopEnd") { if (ParseInt(value, integer) && integer >= 0 && integer <= kMaxTick) s.loopEnd = integer; else Error("Invalid LoopEnd"); }
        else if (key == "Polyphony") { if (ParseInt(value, integer)) s.polyphony = (int)integer; else Error("Invalid Polyphony"); }
        else if (key == "LayerFade") { if (ParseDouble(value, number)) s.layerFade = (float)number; else Error("Invalid LayerFade"); }
        else if (key == "Volume") { if (ParseDouble(value, number)) s.volume = std::max(0.0f, (float)number); else Error("Invalid Volume"); }
        else Warn("Unknown header key '" + key + "'");
    }

    void ParseLayerLine(const std::string& text) {
        Layer& layer = result.sequence.layers[(size_t)currentLayer];
        size_t eq = text.find('=');
        if (eq != std::string::npos && text.find(' ') > eq) {
            std::string key = text.substr(0, eq);
            std::string value = Trim(text.substr(eq + 1));
            double number = 0.0;
            bool flag = false;
            if (key == "Volume") { if (ParseDouble(value, number)) layer.volume = std::max(0.0f, (float)number); else Error("Invalid Volume"); }
            else if (key == "Pan") { if (ParseDouble(value, number)) layer.pan = std::clamp((float)number, -1.0f, 1.0f); else Error("Invalid Pan"); }
            else if (key == "Muted") { if (ParseBool(value, flag)) layer.muted = flag; else Error("Invalid Muted"); }
            else Warn("Unknown layer key '" + key + "'");
            return;
        }

        std::vector<std::string> t = Split(text);
        if (t.empty()) return;
        int instrumentCount = (int)result.sequence.instruments.size();
        if (t[0] == "n") {
            int64_t inst = 0, tick = 0, length = 0, key = 0, vel = 0;
            if (t.size() != 6 || !ParseInt(t[1], inst) || !ParseInt(t[2], tick) || !ParseInt(t[3], length) || !ParseInt(t[4], key) || !ParseInt(t[5], vel)) {
                Error("Malformed note line (expected: n <instrument> <tick> <length> <key> <velocity>)");
                return;
            }
            if (inst < 0 || inst >= instrumentCount) { Error("Note uses unknown instrument " + std::to_string(inst)); return; }
            if (tick < 0 || tick > kMaxTick || length > kMaxTick) { Error("Note tick or length is out of range"); return; }
            if (key < 0 || key > 127) { Error("Note key out of range 0-127"); return; }
            if (vel < 1 || vel > 127) { Error("Note velocity out of range 1-127"); return; }
            if (length < 1) { Warn("Note length below 1 tick was raised to 1"); length = 1; }
            Note note;
            note.instrument = (int)inst;
            note.tick = tick;
            note.length = length;
            note.key = (int)key;
            note.velocity = (int)vel;
            layer.notes.push_back(note);
            return;
        }

        ControllerType type;
        int lo = 0, hi = 127;
        if (t[0] == "v") type = ControllerType::Volume;
        else if (t[0] == "x") type = ControllerType::Expression;
        else if (t[0] == "p") { type = ControllerType::Pan; lo = -64; hi = 63; }
        else if (t[0] == "b") { type = ControllerType::Bend; lo = -8192; hi = 8191; }
        else if (t[0] == "s") type = ControllerType::Sustain;
        else if (t[0] == "r") type = ControllerType::ReverbSend;
        else if (t[0] == "c") type = ControllerType::ChorusSend;
        else { Warn("Unknown layer line '" + t[0] + "' ignored"); return; }

        int64_t inst = 0, tick = 0, value = 0;
        if (t.size() != 4 || !ParseInt(t[1], inst) || !ParseInt(t[2], tick) || !ParseInt(t[3], value)) {
            Error("Malformed controller line (expected: " + t[0] + " <instrument> <tick> <value>)");
            return;
        }
        if (inst < 0 || inst >= instrumentCount) { Error("Controller uses unknown instrument " + std::to_string(inst)); return; }
        if (tick < 0 || tick > kMaxTick) { Error("Controller tick is out of range"); return; }
        if (value < lo || value > hi) {
            Warn("Controller value " + std::to_string(value) + " was clamped to " + std::to_string(lo) + ".." + std::to_string(hi));
            value = std::clamp<int64_t>(value, lo, hi);
        }
        layer.controllers.push_back({ type, (int)inst, tick, (int)value });
    }

    void StartSection(const std::string& title) {
        Sequence& s = result.sequence;
        currentInstrument = -1;
        currentLayer = -1;
        if (title == "Tempo") { section = Section::Tempo; return; }
        if (title == "Meter") { section = Section::Meter; return; }
        if (title == "Markers") { section = Section::Markers; return; }
        if (title.rfind("Instrument ", 0) == 0) {
            int64_t index = 0;
            if (!ParseInt(Trim(title.substr(11)), index) || index != (int64_t)s.instruments.size()) {
                Error("Instruments must be numbered in order starting at 0 (expected " + std::to_string(s.instruments.size()) + ")");
                section = Section::Unknown;
                return;
            }
            s.instruments.push_back(Instrument());
            currentInstrument = (int)s.instruments.size() - 1;
            section = Section::Instrument;
            return;
        }
        if (title.rfind("Layer ", 0) == 0) {
            std::string name = Trim(title.substr(6));
            if (name.empty()) { Error("Layer has no name"); section = Section::Unknown; return; }
            if (name.find('/') != std::string::npos) {
                std::replace(name.begin(), name.end(), '/', '-');
                Warn("'/' in a layer name was replaced with '-'");
            }
            if (s.FindLayer(name) >= 0) {
                std::string unique = name;
                for (int n = 2; s.FindLayer(unique) >= 0; n++) unique = name + " " + std::to_string(n);
                Warn("Duplicate layer name '" + name + "' was renamed to '" + unique + "'");
                name = unique;
            }
            Layer layer;
            layer.name = name;
            s.layers.push_back(layer);
            currentLayer = (int)s.layers.size() - 1;
            section = Section::Layer;
            return;
        }
        Warn("Unknown section [" + title + "] ignored");
        section = Section::Unknown;
    }

    void HandleLine(const std::string& raw) {
        std::string text = Trim(raw);
        if (text.empty() || text[0] == '#') return;

        if (text[0] == '[' && text.back() == ']') {
            StartSection(Trim(text.substr(1, text.size() - 2)));
            return;
        }

        switch (section) {
            case Section::Header:
            case Section::Instrument: {
                size_t eq = text.find('=');
                if (eq == std::string::npos) { Error("Expected key=value"); return; }
                std::string key = Trim(text.substr(0, eq));
                std::string value = Trim(text.substr(eq + 1));
                if (section == Section::Header) ParseHeaderKey(key, value);
                else ParseInstrumentKey(key, value);
                break;
            }
            case Section::Tempo: {
                std::vector<std::string> t = Split(text);
                int64_t tick = 0;
                double bpm = 0.0;
                if (t.size() != 2 || !ParseInt(t[0], tick) || !ParseDouble(t[1], bpm) || tick < 0 || tick > kMaxTick || bpm < 1.0 || bpm > 1000.0) {
                    Error("Malformed tempo line (expected: <tick> <bpm 1-1000>)");
                    return;
                }
                result.sequence.tempo.push_back({ tick, bpm });
                break;
            }
            case Section::Meter: {
                std::vector<std::string> t = Split(text);
                int64_t tick = 0, num = 0, den = 0;
                size_t slash = t.size() == 2 ? t[1].find('/') : std::string::npos;
                if (t.size() != 2 || slash == std::string::npos || !ParseInt(t[0], tick) ||
                    !ParseInt(t[1].substr(0, slash), num) || !ParseInt(t[1].substr(slash + 1), den) ||
                    tick < 0 || tick > kMaxTick || num < 1 || num > 64 || (den != 1 && den != 2 && den != 4 && den != 8 && den != 16 && den != 32)) {
                    Error("Malformed meter line (expected: <tick> <numerator>/<denominator>)");
                    return;
                }
                result.sequence.meter.push_back({ tick, (int)num, (int)den });
                break;
            }
            case Section::Markers: {
                size_t space = text.find_first_of(" \t");
                int64_t tick = 0;
                if (space == std::string::npos || !ParseInt(text.substr(0, space), tick) || tick < 0 || tick > kMaxTick) {
                    Error("Malformed marker line (expected: <tick> <name>)");
                    return;
                }
                std::string name = Trim(text.substr(space));
                if (name.empty()) { Error("Marker has no name"); return; }
                result.sequence.markers.push_back({ tick, name });
                break;
            }
            case Section::Layer:
                ParseLayerLine(text);
                break;
            case Section::Unknown:
                break;
        }
    }
};

}

bool ParseResult::HasErrors() const {
    for (const ParseDiagnostic& d : diagnostics) if (d.error) return true;
    return false;
}

ParseResult ParseSequence(const std::string& text) {
    Parser parser;
    std::istringstream in(text);
    std::string raw;
    bool sawHeader = false;

    while (std::getline(in, raw)) {
        parser.line++;
        std::string trimmed = Trim(raw);
        if (!sawHeader) {
            if (trimmed.empty() || trimmed[0] == '#') continue;
            std::vector<std::string> t = Split(trimmed);
            int64_t version = 0;
            if (t.size() < 2 || t[0] != "StarSequence" || !ParseInt(t[1], version)) {
                parser.Error("Not a StarSequence file (the first line must be: StarSequence <version>)");
                parser.result.ok = false;
                return parser.result;
            }
            if (version > kFormatVersion) parser.Warn("This file uses a newer format version (" + std::to_string(version) + "); some data may be ignored");
            sawHeader = true;
            continue;
        }
        parser.HandleLine(raw);
    }

    if (!sawHeader) {
        parser.Error("The file is empty");
        parser.result.ok = false;
        return parser.result;
    }

    Sequence& s = parser.result.sequence;
    for (Instrument& inst : s.instruments) {
        if (inst.sets.empty()) inst.sets.push_back(SampleSet());
        if (inst.name.empty()) inst.name = "Instrument " + std::to_string(&inst - &s.instruments[0]);
    }
    for (size_t i = 0; i < s.instruments.size(); i++) {
        std::string base = s.instruments[i].name;
        std::string unique = base;
        for (int n = 2;; n++) {
            bool clash = false;
            for (size_t j = 0; j < i; j++) clash |= s.instruments[j].name == unique;
            if (!clash) break;
            unique = base + " " + std::to_string(n);
        }
        if (unique != base) {
            parser.result.diagnostics.push_back({ 0, false, "Duplicate instrument name '" + base + "' was renamed to '" + unique + "'" });
            s.instruments[i].name = unique;
        }
    }

    s.Normalize();
    parser.result.ok = !parser.result.HasErrors();
    return parser.result;
}

const char* ControllerCode(ControllerType type) {
    switch (type) {
        case ControllerType::Volume: return "v";
        case ControllerType::Expression: return "x";
        case ControllerType::Pan: return "p";
        case ControllerType::Bend: return "b";
        case ControllerType::Sustain: return "s";
        case ControllerType::ReverbSend: return "r";
        default: return "c";
    }
}

std::string WriteSequence(const Sequence& s) {
    std::string out;
    out += "StarSequence " + std::to_string(kFormatVersion) + "\n";
    out += "Name=" + s.name + "\n";
    if (s.bus != "Music") out += "Bus=" + s.bus + "\n";
    out += "Resolution=" + std::to_string(s.resolution) + "\n";
    out += "Length=" + std::to_string(s.length) + "\n";
    if (s.loopEnd > 0) {
        out += "LoopStart=" + std::to_string(s.loopStart) + "\n";
        out += "LoopEnd=" + std::to_string(s.loopEnd) + "\n";
    }
    if (s.polyphony != 64) out += "Polyphony=" + std::to_string(s.polyphony) + "\n";
    if (std::fabs(s.layerFade - 0.2f) > 1e-6f) out += "LayerFade=" + FormatNumber(s.layerFade) + "\n";
    if (std::fabs(s.volume - 1.0f) > 1e-6f) out += "Volume=" + FormatNumber(s.volume) + "\n";

    out += "\n[Tempo]\n";
    for (const TempoPoint& p : s.tempo) out += std::to_string(p.tick) + " " + FormatNumber(p.bpm) + "\n";
    out += "\n[Meter]\n";
    for (const MeterPoint& m : s.meter) out += std::to_string(m.tick) + " " + std::to_string(m.numerator) + "/" + std::to_string(m.denominator) + "\n";
    if (!s.markers.empty()) {
        out += "\n[Markers]\n";
        for (const Marker& m : s.markers) out += std::to_string(m.tick) + " " + m.name + "\n";
    }

    const SampleSet defaults;
    for (size_t i = 0; i < s.instruments.size(); i++) {
        const Instrument& inst = s.instruments[i];
        out += "\n[Instrument " + std::to_string(i) + "]\n";
        out += "Name=" + inst.name + "\n";
        bool singleDefault = inst.sets.size() == 1 && inst.sets[0].name == "Default";
        for (const SampleSet& set : inst.sets) {
            if (!singleDefault) out += "Set=" + set.name + "\n";
            if (std::fabs(set.volume - defaults.volume) > 1e-6f) out += "Volume=" + FormatNumber(set.volume) + "\n";
            if (std::fabs(set.pan - defaults.pan) > 1e-6f) out += "Pan=" + FormatNumber(set.pan) + "\n";
            if (set.bendRange != defaults.bendRange) out += "BendRange=" + std::to_string(set.bendRange) + "\n";
            if (std::fabs(set.attack - defaults.attack) > 1e-7f) out += "Attack=" + FormatNumber(set.attack) + "\n";
            if (std::fabs(set.decay - defaults.decay) > 1e-7f) out += "Decay=" + FormatNumber(set.decay) + "\n";
            if (std::fabs(set.sustain - defaults.sustain) > 1e-7f) out += "Sustain=" + FormatNumber(set.sustain) + "\n";
            if (std::fabs(set.release - defaults.release) > 1e-7f) out += "Release=" + FormatNumber(set.release) + "\n";
            for (const SampleZone& z : set.zones) {
                out += "Sample=" + z.assetPath + " keys=" + std::to_string(z.keyLow) + "-" + std::to_string(z.keyHigh);
                if (z.velLow != 1 || z.velHigh != 127) out += " vel=" + std::to_string(z.velLow) + "-" + std::to_string(z.velHigh);
                out += " root=" + std::to_string(z.rootKey);
                switch (z.loop) {
                    case LoopMode::None: out += " loop=none"; break;
                    case LoopMode::Whole: out += " loop=whole"; break;
                    case LoopMode::Wav: out += " loop=wav"; break;
                    case LoopMode::Region: out += " loop=region:" + std::to_string(z.loopStart) + "-" + std::to_string(z.loopEnd); break;
                }
                out += std::string(" pitched=") + (z.pitched ? "true" : "false");
                if (std::fabs(z.tuneCents) > 1e-6f) out += " tune=" + FormatNumber(z.tuneCents);
                if (std::fabs(z.gainDb) > 1e-6f) out += " gain=" + FormatNumber(z.gainDb);
                if (std::fabs(z.pan) > 1e-6f) out += " pan=" + FormatNumber(z.pan);
                out += "\n";
            }
        }
    }

    for (const Layer& layer : s.layers) {
        out += "\n[Layer " + layer.name + "]\n";
        if (std::fabs(layer.volume - 1.0f) > 1e-6f) out += "Volume=" + FormatNumber(layer.volume) + "\n";
        if (std::fabs(layer.pan) > 1e-6f) out += "Pan=" + FormatNumber(layer.pan) + "\n";
        if (layer.muted) out += "Muted=true\n";
        for (const ControllerEvent& c : layer.controllers) {
            out += std::string(ControllerCode(c.type)) + " " + std::to_string(c.instrument) + " " + std::to_string(c.tick) + " " + std::to_string(c.value) + "\n";
        }
        for (const Note& n : layer.notes) {
            out += "n " + std::to_string(n.instrument) + " " + std::to_string(n.tick) + " " + std::to_string(n.length) + " " +
                   std::to_string(n.key) + " " + std::to_string(n.velocity) + "\n";
        }
    }
    return out;
}

bool LoadSequenceFile(const std::string& path, ParseResult& result) {
    std::string resolved = CookedFile::Open(path);
    std::ifstream in(resolved.empty() ? path : resolved, std::ios::binary);
    if (!in.is_open()) {
        result = ParseResult();
        result.diagnostics.push_back({ 0, true, "Could not open " + path });
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    result = ParseSequence(text);
    return result.ok;
}

bool SaveSequenceFile(const std::string& path, const Sequence& sequence, std::string& error) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) { error = "Could not write " + path; return false; }
    out << WriteSequence(sequence);
    if (!out.good()) { error = "Failed while writing " + path; return false; }
    return true;
}

Sequence MakeDefaultSequence(const std::string& name) {
    Sequence s;
    s.name = name;
    s.tempo.push_back({ 0, 120.0 });
    s.meter.push_back({ 0, 4, 4 });
    Instrument inst;
    inst.name = "Instrument 0";
    inst.sets.push_back(SampleSet());
    s.instruments.push_back(inst);
    Layer layer;
    layer.name = "Layer 1";
    s.layers.push_back(layer);
    s.Normalize();
    return s;
}

}
