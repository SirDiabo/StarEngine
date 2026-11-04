#include "AudioEffectTypes.h"
#include <algorithm>
#include <cmath>

namespace {

AudioEffectParamDef P(const char* id, const char* label, float minValue, float maxValue, float defaultValue, const char* unit = "", bool integer = false) {
    AudioEffectParamDef def;
    def.id = id;
    def.label = label;
    def.minValue = minValue;
    def.maxValue = maxValue;
    def.defaultValue = defaultValue;
    def.unit = unit;
    def.integer = integer;
    return def;
}

AudioEffectParamDef Choice(const char* id, const char* label, std::vector<std::string> choices, int defaultIndex) {
    AudioEffectParamDef def;
    def.id = id;
    def.label = label;
    def.minValue = 0.0f;
    def.maxValue = (float)(choices.size() - 1);
    def.defaultValue = (float)defaultIndex;
    def.integer = true;
    def.choices = std::move(choices);
    return def;
}

std::vector<AudioEffectTypeInfo> Build() {
    std::vector<AudioEffectTypeInfo> list;

    list.push_back({ "Reverb", "Reverb", {
        P("mix", "Mix", 0.0f, 1.0f, 0.3f),
        P("room_size", "Room Size", 0.0f, 1.0f, 0.7f),
        P("damping", "Damping", 0.0f, 1.0f, 0.5f),
        P("pre_delay", "Pre-Delay", 0.0f, 200.0f, 20.0f, "ms"),
        P("width", "Width", 0.0f, 1.0f, 1.0f),
    } });

    list.push_back({ "Chorus", "Chorus", {
        P("mix", "Mix", 0.0f, 1.0f, 0.5f),
        P("rate", "Rate", 0.05f, 8.0f, 0.8f, "Hz"),
        P("depth", "Depth", 0.0f, 10.0f, 3.0f, "ms"),
        P("delay", "Delay", 5.0f, 30.0f, 15.0f, "ms"),
        P("feedback", "Feedback", 0.0f, 0.7f, 0.0f),
    } });

    list.push_back({ "Delay", "Delay", {
        P("mix", "Mix", 0.0f, 1.0f, 0.35f),
        P("time", "Time", 1.0f, 2000.0f, 350.0f, "ms"),
        P("feedback", "Feedback", 0.0f, 0.95f, 0.4f),
        P("tone", "Tone", 200.0f, 20000.0f, 6000.0f, "Hz"),
        P("ping_pong", "Ping-Pong", 0.0f, 1.0f, 1.0f, "", true),
    } });

    list.push_back({ "Filter", "Filter", {
        P("mix", "Mix", 0.0f, 1.0f, 1.0f),
        Choice("mode", "Mode", { "LowPass", "HighPass", "BandPass" }, 0),
        P("cutoff", "Cutoff", 20.0f, 20000.0f, 1000.0f, "Hz"),
        P("resonance", "Resonance", 0.1f, 10.0f, 0.707f),
    } });

    list.push_back({ "EQ", "3-Band EQ", {
        P("mix", "Mix", 0.0f, 1.0f, 1.0f),
        P("low_gain", "Low Gain", -18.0f, 18.0f, 0.0f, "dB"),
        P("low_freq", "Low Freq", 50.0f, 500.0f, 200.0f, "Hz"),
        P("mid_gain", "Mid Gain", -18.0f, 18.0f, 0.0f, "dB"),
        P("mid_freq", "Mid Freq", 200.0f, 5000.0f, 1000.0f, "Hz"),
        P("mid_q", "Mid Q", 0.1f, 5.0f, 1.0f),
        P("high_gain", "High Gain", -18.0f, 18.0f, 0.0f, "dB"),
        P("high_freq", "High Freq", 2000.0f, 16000.0f, 5000.0f, "Hz"),
    } });

    list.push_back({ "Compressor", "Compressor", {
        P("mix", "Mix", 0.0f, 1.0f, 1.0f),
        P("threshold", "Threshold", -60.0f, 0.0f, -18.0f, "dB"),
        P("ratio", "Ratio", 1.0f, 20.0f, 4.0f),
        P("attack", "Attack", 0.1f, 200.0f, 10.0f, "ms"),
        P("release", "Release", 5.0f, 2000.0f, 120.0f, "ms"),
        P("makeup", "Makeup Gain", 0.0f, 24.0f, 0.0f, "dB"),
        P("knee", "Knee", 0.0f, 12.0f, 6.0f, "dB"),
    } });

    list.push_back({ "Distortion", "Distortion", {
        P("mix", "Mix", 0.0f, 1.0f, 1.0f),
        P("drive", "Drive", 0.0f, 40.0f, 12.0f, "dB"),
        P("tone", "Tone", 500.0f, 20000.0f, 8000.0f, "Hz"),
        P("bit_depth", "Bit Depth", 2.0f, 24.0f, 24.0f, "bits", true),
        P("output", "Output", -24.0f, 6.0f, 0.0f, "dB"),
    } });

    return list;
}

}

namespace AudioEffectTypes {

const std::vector<AudioEffectTypeInfo>& All() {
    static const std::vector<AudioEffectTypeInfo> list = Build();
    return list;
}

const AudioEffectTypeInfo* Find(const std::string& type) {
    for (const AudioEffectTypeInfo& info : All()) {
        if (info.type == type) return &info;
    }
    return nullptr;
}

int ParamIndex(const AudioEffectTypeInfo& info, const std::string& paramId) {
    for (size_t i = 0; i < info.params.size(); i++) {
        if (info.params[i].id == paramId) return (int)i;
    }
    return -1;
}

float ClampParam(const AudioEffectParamDef& def, float value) {
    if (!std::isfinite(value)) return def.defaultValue;
    value = std::clamp(value, def.minValue, def.maxValue);
    if (def.integer) value = std::round(value);
    return value;
}

}
