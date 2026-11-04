#include "AudioEffects.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

inline float Flush(float v) {
    return std::fabs(v) < 1e-20f ? 0.0f : v;
}

float DbToGain(float db) {
    return std::pow(10.0f, db / 20.0f);
}

struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1[2] = { 0.0f, 0.0f };
    float z2[2] = { 0.0f, 0.0f };

    void Reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    void Set(float nb0, float nb1, float nb2, float na0, float na1, float na2) {
        float inv = 1.0f / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }

    static float ClampFreq(float f, int rate) {
        return std::clamp(f, 10.0f, (float)rate * 0.45f);
    }

    void LowPass(float f, float q, int rate) {
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set((1.0f - c) * 0.5f, 1.0f - c, (1.0f - c) * 0.5f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }

    void HighPass(float f, float q, int rate) {
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set((1.0f + c) * 0.5f, -(1.0f + c), (1.0f + c) * 0.5f, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }

    void BandPass(float f, float q, int rate) {
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set(alpha, 0.0f, -alpha, 1.0f + alpha, -2.0f * c, 1.0f - alpha);
    }

    void Peak(float f, float q, float gainDb, int rate) {
        float a = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        Set(1.0f + alpha * a, -2.0f * c, 1.0f - alpha * a, 1.0f + alpha / a, -2.0f * c, 1.0f - alpha / a);
    }

    void LowShelf(float f, float gainDb, int rate) {
        float a = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / 2.0f * std::sqrt(2.0f);
        float sq = 2.0f * std::sqrt(a) * alpha;
        Set(a * ((a + 1.0f) - (a - 1.0f) * c + sq),
            2.0f * a * ((a - 1.0f) - (a + 1.0f) * c),
            a * ((a + 1.0f) - (a - 1.0f) * c - sq),
            (a + 1.0f) + (a - 1.0f) * c + sq,
            -2.0f * ((a - 1.0f) + (a + 1.0f) * c),
            (a + 1.0f) + (a - 1.0f) * c - sq);
    }

    void HighShelf(float f, float gainDb, int rate) {
        float a = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * ClampFreq(f, rate) / (float)rate;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / 2.0f * std::sqrt(2.0f);
        float sq = 2.0f * std::sqrt(a) * alpha;
        Set(a * ((a + 1.0f) + (a - 1.0f) * c + sq),
            -2.0f * a * ((a - 1.0f) + (a + 1.0f) * c),
            a * ((a + 1.0f) + (a - 1.0f) * c - sq),
            (a + 1.0f) - (a - 1.0f) * c + sq,
            2.0f * ((a - 1.0f) - (a + 1.0f) * c),
            (a + 1.0f) - (a - 1.0f) * c - sq);
    }

    float Tick(float x, int ch) {
        float y = b0 * x + z1[ch];
        z1[ch] = Flush(b1 * x - a1 * y + z2[ch]);
        z2[ch] = Flush(b2 * x - a2 * y);
        return y;
    }
};

struct DelayLine {
    std::vector<float> buf;
    size_t pos = 0;

    void Allocate(size_t size) { buf.assign(std::max<size_t>(size, 4), 0.0f); pos = 0; }
    void Clear() { std::fill(buf.begin(), buf.end(), 0.0f); pos = 0; }
    void Write(float v) { buf[pos] = v; pos++; if (pos >= buf.size()) pos = 0; }

    float Read(float delay) const {
        float maxDelay = (float)buf.size() - 2.0f;
        delay = std::clamp(delay, 1.0f, maxDelay);
        float readPos = (float)pos - delay;
        if (readPos < 0.0f) readPos += (float)buf.size();
        if (readPos >= (float)buf.size()) readPos -= (float)buf.size();
        size_t i0 = (size_t)readPos;
        if (i0 >= buf.size()) i0 = 0;
        size_t i1 = i0 + 1 >= buf.size() ? 0 : i0 + 1;
        float frac = readPos - (float)i0;
        return buf[i0] + (buf[i1] - buf[i0]) * frac;
    }
};

class ReverbEffect : public AudioEffect {
public:
    ReverbEffect(const AudioEffectTypeInfo* info, int sampleRate) : AudioEffect(info, sampleRate) {
        static const int combTuning[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int allpassTuning[4] = { 556, 441, 341, 225 };
        const double scale = (double)sampleRate / 44100.0;
        const int spread = std::max(1, (int)std::lround(23.0 * scale));
        for (int ch = 0; ch < 2; ch++) {
            for (int i = 0; i < 8; i++) {
                size_t size = (size_t)std::max(8L, std::lround(combTuning[i] * scale)) + (size_t)(ch * spread);
                comb[ch][i].buf.assign(size, 0.0f);
            }
            for (int i = 0; i < 4; i++) {
                size_t size = (size_t)std::max(4L, std::lround(allpassTuning[i] * scale)) + (size_t)(ch * spread);
                allpass[ch][i].buf.assign(size, 0.0f);
            }
            preDelay[ch].Allocate((size_t)(sampleRate * 0.2) + 8);
        }
    }

protected:
    void Update() override {
        feedback = P(ReverbParam::RoomSize) * 0.28f + 0.7f;
        damp1 = P(ReverbParam::Damping) * 0.4f;
        damp2 = 1.0f - damp1;
        float width = P(ReverbParam::Width);
        wet1 = width * 0.5f + 0.5f;
        wet2 = (1.0f - width) * 0.5f;
        preDelaySamples = P(ReverbParam::PreDelay) * 0.001f * (float)rate;
    }

    void ResetState() override {
        for (int ch = 0; ch < 2; ch++) {
            for (auto& c : comb[ch]) { std::fill(c.buf.begin(), c.buf.end(), 0.0f); c.idx = 0; c.store = 0.0f; }
            for (auto& a : allpass[ch]) { std::fill(a.buf.begin(), a.buf.end(), 0.0f); a.idx = 0; }
            preDelay[ch].Clear();
        }
    }

    void ProcessWet(float* io, int frames) override {
        constexpr float kInputGain = 0.015f;
        constexpr float kWetScale = 2.0f;
        for (int i = 0; i < frames; i++) {
            float l = preDelaySamples < 1.0f ? io[i * 2] : preDelay[0].Read(preDelaySamples);
            float r = preDelaySamples < 1.0f ? io[i * 2 + 1] : preDelay[1].Read(preDelaySamples);
            preDelay[0].Write(io[i * 2]);
            preDelay[1].Write(io[i * 2 + 1]);
            float input = (l + r) * kInputGain;

            float out[2] = { 0.0f, 0.0f };
            for (int ch = 0; ch < 2; ch++) {
                float sum = 0.0f;
                for (auto& c : comb[ch]) {
                    float y = c.buf[c.idx];
                    c.store = Flush(y * damp2 + c.store * damp1);
                    c.buf[c.idx] = Flush(input + c.store * feedback);
                    if (++c.idx >= c.buf.size()) c.idx = 0;
                    sum += y;
                }
                for (auto& a : allpass[ch]) {
                    float buffered = a.buf[a.idx];
                    float y = -sum + buffered;
                    a.buf[a.idx] = Flush(sum + buffered * 0.5f);
                    if (++a.idx >= a.buf.size()) a.idx = 0;
                    sum = y;
                }
                out[ch] = sum;
            }
            io[i * 2] = (out[0] * wet1 + out[1] * wet2) * kWetScale;
            io[i * 2 + 1] = (out[1] * wet1 + out[0] * wet2) * kWetScale;
        }
    }

private:
    struct Comb { std::vector<float> buf; size_t idx = 0; float store = 0.0f; };
    struct Allpass { std::vector<float> buf; size_t idx = 0; };

    Comb comb[2][8];
    Allpass allpass[2][4];
    DelayLine preDelay[2];
    float feedback = 0.84f, damp1 = 0.2f, damp2 = 0.8f, wet1 = 1.0f, wet2 = 0.0f, preDelaySamples = 0.0f;
};

class ChorusEffect : public AudioEffect {
public:
    ChorusEffect(const AudioEffectTypeInfo* info, int sampleRate) : AudioEffect(info, sampleRate) {
        for (int ch = 0; ch < 2; ch++) line[ch].Allocate((size_t)(sampleRate * 0.06) + 8);
    }

protected:
    void Update() override {
        phaseStep = P(ChorusParam::Rate) / (float)rate;
        depthSamples = P(ChorusParam::Depth) * 0.001f * (float)rate;
        baseSamples = P(ChorusParam::Delay) * 0.001f * (float)rate;
        feedback = P(ChorusParam::Feedback);
    }

    void ResetState() override {
        line[0].Clear();
        line[1].Clear();
        phase = 0.0f;
    }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            float lfoL = std::sin(2.0f * kPi * phase);
            float lfoR = std::sin(2.0f * kPi * (phase + 0.25f));
            float wl = line[0].Read(baseSamples + depthSamples * lfoL);
            float wr = line[1].Read(baseSamples + depthSamples * lfoR);
            line[0].Write(Flush(io[i * 2] + feedback * wl));
            line[1].Write(Flush(io[i * 2 + 1] + feedback * wr));
            io[i * 2] = wl;
            io[i * 2 + 1] = wr;
            phase += phaseStep;
            if (phase >= 1.0f) phase -= 1.0f;
        }
    }

private:
    DelayLine line[2];
    float phase = 0.0f, phaseStep = 0.0f, depthSamples = 0.0f, baseSamples = 0.0f, feedback = 0.0f;
};

class DelayEffect : public AudioEffect {
public:
    DelayEffect(const AudioEffectTypeInfo* info, int sampleRate) : AudioEffect(info, sampleRate) {
        for (int ch = 0; ch < 2; ch++) line[ch].Allocate((size_t)(sampleRate * 2.0) + 16);
    }

protected:
    void Update() override {
        targetDelay = P(DelayParam::Time) * 0.001f * (float)rate;
        if (!delayPrimed) { delaySamples = targetDelay; delayPrimed = true; }
        feedback = P(DelayParam::Feedback);
        toneCoef = std::exp(-2.0f * kPi * P(DelayParam::Tone) / (float)rate);
        pingPong = P(DelayParam::PingPong) >= 0.5f;
    }

    void ResetState() override {
        line[0].Clear();
        line[1].Clear();
        tone[0] = tone[1] = 0.0f;
        delayPrimed = false;
    }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            delaySamples += (targetDelay - delaySamples) * 0.0008f;
            float dl = line[0].Read(delaySamples);
            float dr = line[1].Read(delaySamples);
            tone[0] = Flush(dl * (1.0f - toneCoef) + tone[0] * toneCoef);
            tone[1] = Flush(dr * (1.0f - toneCoef) + tone[1] * toneCoef);
            float inL = io[i * 2], inR = io[i * 2 + 1];
            if (pingPong) {
                line[0].Write(Flush(0.5f * (inL + inR) + feedback * tone[1]));
                line[1].Write(Flush(feedback * tone[0]));
            } else {
                line[0].Write(Flush(inL + feedback * tone[0]));
                line[1].Write(Flush(inR + feedback * tone[1]));
            }
            io[i * 2] = dl;
            io[i * 2 + 1] = dr;
        }
    }

private:
    DelayLine line[2];
    float tone[2] = { 0.0f, 0.0f };
    float delaySamples = 0.0f, targetDelay = 0.0f, feedback = 0.0f, toneCoef = 0.0f;
    bool delayPrimed = false;
    bool pingPong = true;
};

class FilterEffect : public AudioEffect {
public:
    using AudioEffect::AudioEffect;

protected:
    void Update() override {
        float f = P(FilterParam::Cutoff), q = P(FilterParam::Resonance);
        int mode = (int)P(FilterParam::Mode);
        if (mode == 1) filter.HighPass(f, q, rate);
        else if (mode == 2) filter.BandPass(f, q, rate);
        else filter.LowPass(f, q, rate);
    }

    void ResetState() override { filter.Reset(); }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            io[i * 2] = filter.Tick(io[i * 2], 0);
            io[i * 2 + 1] = filter.Tick(io[i * 2 + 1], 1);
        }
    }

private:
    Biquad filter;
};

class EqEffect : public AudioEffect {
public:
    using AudioEffect::AudioEffect;

protected:
    void Update() override {
        low.LowShelf(P(EqParam::LowFreq), P(EqParam::LowGain), rate);
        mid.Peak(P(EqParam::MidFreq), P(EqParam::MidQ), P(EqParam::MidGain), rate);
        high.HighShelf(P(EqParam::HighFreq), P(EqParam::HighGain), rate);
    }

    void ResetState() override { low.Reset(); mid.Reset(); high.Reset(); }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            for (int ch = 0; ch < 2; ch++) {
                float x = io[i * 2 + ch];
                x = low.Tick(x, ch);
                x = mid.Tick(x, ch);
                x = high.Tick(x, ch);
                io[i * 2 + ch] = x;
            }
        }
    }

private:
    Biquad low, mid, high;
};

class CompressorEffect : public AudioEffect {
public:
    using AudioEffect::AudioEffect;

protected:
    void Update() override {
        threshold = P(CompressorParam::Threshold);
        ratio = P(CompressorParam::Ratio);
        knee = P(CompressorParam::Knee);
        makeup = P(CompressorParam::Makeup);
        attackCoef = std::exp(-1.0f / (std::max(0.01f, P(CompressorParam::Attack)) * 0.001f * (float)rate));
        releaseCoef = std::exp(-1.0f / (std::max(0.01f, P(CompressorParam::Release)) * 0.001f * (float)rate));
    }

    void ResetState() override { envelope = 0.0f; }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            float level = std::max(std::fabs(io[i * 2]), std::fabs(io[i * 2 + 1]));
            float db = 20.0f * std::log10(std::max(level, 1e-9f));
            float over = db - threshold;
            float outDb;
            if (2.0f * over < -knee) outDb = db;
            else if (knee > 0.0f && 2.0f * std::fabs(over) <= knee) {
                float t = over + knee * 0.5f;
                outDb = db + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
            } else outDb = threshold + over / ratio;

            float wanted = outDb - db;
            float coef = wanted < envelope ? attackCoef : releaseCoef;
            envelope = Flush(coef * envelope + (1.0f - coef) * wanted);
            float gain = DbToGain(envelope + makeup);
            io[i * 2] *= gain;
            io[i * 2 + 1] *= gain;
        }
    }

private:
    float threshold = -18.0f, ratio = 4.0f, knee = 6.0f, makeup = 0.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, envelope = 0.0f;
};

class DistortionEffect : public AudioEffect {
public:
    using AudioEffect::AudioEffect;

protected:
    void Update() override {
        drive = DbToGain(P(DistortionParam::Drive));
        outGain = DbToGain(P(DistortionParam::Output));
        toneCoef = std::exp(-2.0f * kPi * P(DistortionParam::Tone) / (float)rate);
        int bits = (int)P(DistortionParam::BitDepth);
        crush = bits < 24;
        levels = std::pow(2.0f, (float)(bits - 1));
    }

    void ResetState() override { tone[0] = tone[1] = 0.0f; }

    void ProcessWet(float* io, int frames) override {
        for (int i = 0; i < frames; i++) {
            for (int ch = 0; ch < 2; ch++) {
                float y = std::tanh(io[i * 2 + ch] * drive);
                if (crush) y = std::round(y * levels) / levels;
                tone[ch] = Flush(y * (1.0f - toneCoef) + tone[ch] * toneCoef);
                io[i * 2 + ch] = tone[ch] * outGain;
            }
        }
    }

private:
    float drive = 4.0f, outGain = 1.0f, toneCoef = 0.0f, levels = 8388608.0f;
    float tone[2] = { 0.0f, 0.0f };
    bool crush = false;
};

}

AudioEffect::AudioEffect(const AudioEffectTypeInfo* typeInfo, int sampleRate)
    : rate(sampleRate), info(typeInfo) {
    for (int i = 0; i < AudioEffectTypes::kMaxParams; i++) {
        float v = i < (int)info->params.size() ? info->params[(size_t)i].defaultValue : 0.0f;
        target[i].store(v);
        current[i] = v;
    }
    dry.assign((size_t)kChunkFrames * 2, 0.0f);
}

void AudioEffect::SetParam(int index, float value) {
    if (index < 0 || index >= (int)info->params.size()) return;
    target[index].store(AudioEffectTypes::ClampParam(info->params[(size_t)index], value), std::memory_order_relaxed);
}

float AudioEffect::GetParam(int index) const {
    if (index < 0 || index >= AudioEffectTypes::kMaxParams) return 0.0f;
    return target[index].load(std::memory_order_relaxed);
}

void AudioEffect::Reset() {
    ResetState();
    primed = false;
}

void AudioEffect::Process(float* io, int frames) {
    const int count = (int)info->params.size();
    while (frames > 0) {
        int chunk = std::min(frames, kChunkFrames);

        for (int i = 0; i < chunk * 2; i++) {
            if (!std::isfinite(io[i])) io[i] = 0.0f;
        }

        bool changed = !primed;
        const float smoothing = 1.0f - std::exp(-(float)chunk / ((float)rate * 0.03f));
        for (int i = 0; i < count; i++) {
            float t = target[i].load(std::memory_order_relaxed);
            if (!primed || info->params[(size_t)i].integer) {
                if (current[i] != t) { current[i] = t; changed = true; }
                continue;
            }
            float range = info->params[(size_t)i].maxValue - info->params[(size_t)i].minValue;
            float diff = t - current[i];
            if (diff == 0.0f) continue;
            if (std::fabs(diff) <= range * 1e-4f) current[i] = t;
            else current[i] += diff * smoothing;
            changed = true;
        }
        primed = true;
        if (changed) Update();

        const float mix = current[0];
        const bool blend = mix < 0.9999f;
        if (blend) std::copy(io, io + (size_t)chunk * 2, dry.begin());

        ProcessWet(io, chunk);

        bool bad = false;
        for (int i = 0; i < chunk * 2; i++) {
            if (!std::isfinite(io[i])) { bad = true; break; }
        }
        if (bad) {
            ResetState();
            if (blend) std::copy(dry.begin(), dry.begin() + (size_t)chunk * 2, io);
            else std::fill(io, io + (size_t)chunk * 2, 0.0f);
        } else if (blend) {
            for (int i = 0; i < chunk * 2; i++) io[i] = dry[(size_t)i] * (1.0f - mix) + io[i] * mix;
        }

        io += (size_t)chunk * 2;
        frames -= chunk;
    }
}

std::shared_ptr<AudioEffect> CreateAudioEffect(const std::string& type, int sampleRate) {
    const AudioEffectTypeInfo* info = AudioEffectTypes::Find(type);
    if (!info || sampleRate < 8000) return nullptr;
    if (type == "Reverb") return std::make_shared<ReverbEffect>(info, sampleRate);
    if (type == "Chorus") return std::make_shared<ChorusEffect>(info, sampleRate);
    if (type == "Delay") return std::make_shared<DelayEffect>(info, sampleRate);
    if (type == "Filter") return std::make_shared<FilterEffect>(info, sampleRate);
    if (type == "EQ") return std::make_shared<EqEffect>(info, sampleRate);
    if (type == "Compressor") return std::make_shared<CompressorEffect>(info, sampleRate);
    if (type == "Distortion") return std::make_shared<DistortionEffect>(info, sampleRate);
    return nullptr;
}
