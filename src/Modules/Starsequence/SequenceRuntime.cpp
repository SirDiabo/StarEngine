#include "SequenceRuntime.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace Starsequence {

namespace {

constexpr float kSilence = 0.0001f;

float DbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

}

SequenceControl::SequenceControl(int layerCount, int instrumentCount) : layers(layerCount), instruments(instrumentCount) {
    int l = std::max(layerCount, 1);
    int i = std::max(instrumentCount, 1);
    layerVolume.reset(new std::atomic<float>[(size_t)l]);
    layerPan.reset(new std::atomic<float>[(size_t)l]);
    layerMute.reset(new std::atomic<bool>[(size_t)l]);
    layerSolo.reset(new std::atomic<bool>[(size_t)l]);
    instrumentVolume.reset(new std::atomic<float>[(size_t)i]);
    instrumentSet.reset(new std::atomic<int>[(size_t)i]);
    for (int n = 0; n < l; n++) {
        layerVolume[(size_t)n].store(1.0f);
        layerPan[(size_t)n].store(0.0f);
        layerMute[(size_t)n].store(false);
        layerSolo[(size_t)n].store(false);
    }
    for (int n = 0; n < i; n++) {
        instrumentVolume[(size_t)n].store(1.0f);
        instrumentSet[(size_t)n].store(0);
    }
}

std::shared_ptr<RtSequence> CompileSequence(const Sequence& s, const SampleLoader& loader, CompileReport& report) {
    auto rt = std::make_shared<RtSequence>();
    rt->resolution = s.resolution;
    rt->length = s.length;
    rt->loopStart = s.loopStart;
    rt->loopEnd = s.loopEnd;
    rt->polyphony = s.polyphony;
    rt->tempo = s.tempo;
    if (rt->tempo.empty()) rt->tempo.push_back({ 0, 120.0 });
    rt->markers = s.markers;
    s.BuildBoundaries(rt->beats, rt->bars);

    std::map<std::string, LoadedSample> cache;
    std::map<std::string, bool> failed;

    for (const Instrument& inst : s.instruments) {
        RtInstrument rtInst;
        for (const SampleSet& set : inst.sets) {
            RtSet rtSet;
            rtSet.volume = set.volume;
            rtSet.pan = set.pan;
            rtSet.bendRange = set.bendRange;
            rtSet.attack = set.attack;
            rtSet.decay = set.decay;
            rtSet.sustain = set.sustain;
            rtSet.release = set.release;

            for (const SampleZone& zone : set.zones) {
                RtZone rtZone;
                rtZone.keyLow = zone.keyLow;
                rtZone.keyHigh = zone.keyHigh;
                rtZone.velLow = zone.velLow;
                rtZone.velHigh = zone.velHigh;
                rtZone.rootKey = zone.rootKey;
                rtZone.pitched = zone.pitched;
                rtZone.tuneCents = zone.tuneCents;
                rtZone.pan = zone.pan;

                if (zone.assetPath.empty()) {
                    rtSet.zones.push_back(rtZone);
                    continue;
                }

                auto found = cache.find(zone.assetPath);
                if (found == cache.end() && !failed.count(zone.assetPath)) {
                    LoadedSample loaded;
                    if (loader && loader(zone.assetPath, loaded) && loaded.data && loaded.data->frameCount > 0) {
                        cache[zone.assetPath] = loaded;
                        rt->keepAlive.push_back(loaded.data);
                        found = cache.find(zone.assetPath);
                    } else {
                        failed[zone.assetPath] = true;
                        report.warnings.push_back("Sample '" + zone.assetPath + "' (instrument '" + inst.name + "', set '" + set.name + "') could not be loaded");
                    }
                }
                if (found == cache.end()) {
                    rtSet.zones.push_back(rtZone);
                    continue;
                }

                const SampleData* data = found->second.data.get();
                rtZone.data = data;
                rtZone.gain = DbToLinear(zone.gainDb) * found->second.assetGain;
                if (zone.loop == LoopMode::Whole) {
                    rtZone.looping = true;
                    rtZone.loopStart = 0;
                    rtZone.loopEnd = data->frameCount;
                } else if (zone.loop == LoopMode::Region) {
                    uint64_t end = std::min<uint64_t>(zone.loopEnd, data->frameCount);
                    if (zone.loopStart < end) {
                        rtZone.looping = true;
                        rtZone.loopStart = zone.loopStart;
                        rtZone.loopEnd = end;
                    }
                } else if (zone.loop == LoopMode::Wav && data->hasLoop) {
                    rtZone.looping = true;
                    rtZone.loopStart = data->loopStart;
                    rtZone.loopEnd = data->loopEnd;
                }
                rtSet.zones.push_back(rtZone);
            }
            rtInst.sets.push_back(rtSet);
        }
        rt->instruments.push_back(rtInst);
    }

    for (const Layer& layer : s.layers) {
        RtLayer rtLayer;
        for (const ControllerEvent& c : layer.controllers) {
            RtEvent e;
            e.tick = c.tick;
            e.type = RtEventType::Controller;
            e.instrument = (uint8_t)c.instrument;
            e.controller = (uint8_t)c.type;
            e.value = c.value;
            rtLayer.events.push_back(e);
        }
        for (const Note& n : layer.notes) {
            RtEvent e;
            e.tick = n.tick;
            e.length = n.length;
            e.type = RtEventType::NoteOn;
            e.instrument = (uint8_t)n.instrument;
            e.key = n.key;
            e.velocity = n.velocity;
            rtLayer.events.push_back(e);
        }
        std::stable_sort(rtLayer.events.begin(), rtLayer.events.end(), [](const RtEvent& a, const RtEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return (int)a.type < (int)b.type;
        });
        rt->layers.push_back(std::move(rtLayer));
    }
    return rt;
}

SequenceRenderer::SequenceRenderer(std::shared_ptr<const RtSequence> seq, std::shared_ptr<SequenceControl> ctl)
    : sequence(std::move(seq)), control(std::move(ctl)) {
    int layers = (int)sequence->layers.size();
    int instruments = (int)sequence->instruments.size();
    voices.resize((size_t)(kMaxPolyphony + kExtraDyingVoices));
    cursors.assign((size_t)layers, 0);
    channels.assign((size_t)std::max(1, layers * instruments), ChannelState());
    skipped.assign((size_t)std::max(1, layers * kMaxSkippedNotesPerLayer), SkippedNote());
    for (SkippedNote& s : skipped) s.used = false;
    layerGain.assign((size_t)layers, 1.0f);
    layerTarget.assign((size_t)layers, 1.0f);
    layerAudible.assign((size_t)layers, true);
    layerStartGain.assign((size_t)layers, 1.0f);
    layerEndGain.assign((size_t)layers, 1.0f);
    polyphonyLimit = std::clamp(sequence->polyphony, 1, kMaxPolyphony);
}

void SequenceRenderer::UpdateLayerTargets() {
    anySolo = false;
    for (size_t l = 0; l < layerTarget.size(); l++) {
        if (control->layerSolo[l].load(std::memory_order_relaxed)) anySolo = true;
    }
    for (size_t l = 0; l < layerTarget.size(); l++) {
        bool mute = control->layerMute[l].load(std::memory_order_relaxed);
        bool solo = control->layerSolo[l].load(std::memory_order_relaxed);
        float volume = control->layerVolume[l].load(std::memory_order_relaxed);
        layerTarget[l] = (mute || (anySolo && !solo)) ? 0.0f : std::max(0.0f, volume);
    }
}

void SequenceRenderer::Initialize(int rate) {
    sampleRate = rate;
    initialized = true;
    UpdateLayerTargets();
    for (size_t l = 0; l < layerGain.size(); l++) {
        layerGain[l] = layerTarget[l];
        layerAudible[l] = layerTarget[l] > 0.0f;
    }
    pitchSmoothed = control->pitch.load();
    fadeGain = 1.0f;
    tick = 0.0;
    JumpTo(0, 0.0);
}

void SequenceRenderer::ResetBpmIndex() {
    bpmIndex = 0;
    while (bpmIndex + 1 < sequence->tempo.size() && (double)sequence->tempo[bpmIndex + 1].tick <= tick + 1e-9) bpmIndex++;
}

double SequenceRenderer::TicksPerFrame() const {
    double bpm = sequence->tempo[std::min(bpmIndex, sequence->tempo.size() - 1)].bpm;
    double mult = std::clamp((double)control->tempo.load(std::memory_order_relaxed), 0.0001, 32.0);
    return bpm * (double)sequence->resolution / 60.0 / (double)sampleRate * mult;
}

int64_t SequenceRenderer::NextBoundary(int64_t from, int quantize) const {
    const std::vector<int64_t>& list = quantize == 1 ? sequence->beats : sequence->bars;
    auto it = std::lower_bound(list.begin(), list.end(), from);
    return it == list.end() ? sequence->length : *it;
}

int SequenceRenderer::ActiveCount() const {
    int n = 0;
    for (const Voice& v : voices) if (v.active) n++;
    return n;
}

void SequenceRenderer::ApplyController(int layer, const RtEvent& event, bool live) {
    int index = layer * (int)sequence->instruments.size() + event.instrument;
    if (index < 0 || (size_t)index >= channels.size()) return;
    ChannelState& ch = channels[(size_t)index];
    switch ((ControllerType)event.controller) {
        case ControllerType::Volume: ch.volume = event.value; break;
        case ControllerType::Expression: ch.expression = event.value; break;
        case ControllerType::Pan: ch.pan = event.value; break;
        case ControllerType::Bend: ch.bend = event.value; break;
        case ControllerType::Sustain: {
            bool on = event.value >= 64;
            bool wasOn = ch.sustain;
            ch.sustain = on;
            if (live && wasOn && !on) {
                for (Voice& v : voices) {
                    if (v.active && v.held && v.state == index) {
                        v.held = false;
                        ReleaseVoice(v, false);
                    }
                }
            }
            break;
        }
        default: break;
    }
}

void SequenceRenderer::ReplayControllers(int64_t target) {
    for (ChannelState& ch : channels) ch = ChannelState();
    for (size_t l = 0; l < sequence->layers.size(); l++) {
        for (const RtEvent& e : sequence->layers[l].events) {
            if (e.tick >= target) break;
            if (e.type == RtEventType::Controller) ApplyController((int)l, e, false);
        }
    }
}

void SequenceRenderer::ReleaseVoice(Voice& voice, bool forced) {
    if (!voice.active || voice.releasing) return;
    if (!forced && !voice.zone->pitched) return;
    voice.releasing = true;
    voice.held = false;
    voice.hasNoteEnd = false;
    voice.stage = 3;
    float seconds = std::max(voice.set->release, 0.005f);
    if (forced && !voice.zone->pitched) seconds = 0.03f;
    voice.releaseStep = voice.envelope / std::max(1.0f, seconds * (float)sampleRate);
}

void SequenceRenderer::ReleaseNoteEnds() {
    for (Voice& v : voices) {
        if (!v.active || v.releasing || !v.hasNoteEnd || v.noteEnd > (int64_t)std::ceil(tick - 1e-9)) continue;
        v.hasNoteEnd = false;
        if (channels[(size_t)v.state].sustain && v.zone->pitched) v.held = true;
        else ReleaseVoice(v, false);
    }
}

void SequenceRenderer::ReleaseAllForJump() {
    for (Voice& v : voices) {
        if (v.active && !v.releasing) ReleaseVoice(v, true);
    }
}

void SequenceRenderer::JumpTo(int64_t target, double overshoot) {
    ReleaseAllForJump();
    tick = (double)target + overshoot;
    for (size_t l = 0; l < sequence->layers.size(); l++) {
        const std::vector<RtEvent>& events = sequence->layers[l].events;
        auto it = std::lower_bound(events.begin(), events.end(), target, [](const RtEvent& e, int64_t t) { return e.tick < t; });
        cursors[l] = (size_t)(it - events.begin());
    }
    ReplayControllers(target);
    for (SkippedNote& s : skipped) s.used = false;
    ResetBpmIndex();
    ended = false;
}

SequenceRenderer::Voice* SequenceRenderer::AllocateVoice() {
    int count = 0;
    for (const Voice& v : voices) if (v.active && !v.dying) count++;

    if (count >= polyphonyLimit) {
        Voice* victim = nullptr;
        for (Voice& v : voices) {
            if (!v.active || v.dying) continue;
            if (!victim) { victim = &v; continue; }
            bool vr = v.releasing, br = victim->releasing;
            if (vr != br) { if (vr) victim = &v; continue; }
            if (vr) { if (v.envelope < victim->envelope) victim = &v; }
            else if (v.order < victim->order) victim = &v;
        }
        if (victim) {
            victim->dying = true;
            victim->releasing = true;
            victim->held = false;
            victim->hasNoteEnd = false;
            victim->stage = 3;
            victim->releaseStep = victim->envelope / std::max(1.0f, 0.003f * (float)sampleRate);
        }
    }

    Voice* slot = nullptr;
    for (Voice& v : voices) if (!v.active) { slot = &v; break; }
    if (!slot) {
        for (Voice& v : voices) if (v.dying && (!slot || v.order < slot->order)) slot = &v;
    }
    if (!slot) {
        for (Voice& v : voices) if (!slot || v.order < slot->order) slot = &v;
    }
    *slot = Voice();
    return slot;
}

void SequenceRenderer::SpawnVoices(int layer, int instrument, int key, int velocity, int64_t noteEnd, double elapsedSeconds) {
    if (instrument < 0 || (size_t)instrument >= sequence->instruments.size()) return;
    const RtInstrument& inst = sequence->instruments[(size_t)instrument];
    if (inst.sets.empty()) return;
    int setIndex = std::clamp(control->instrumentSet[(size_t)instrument].load(std::memory_order_relaxed), 0, (int)inst.sets.size() - 1);
    const RtSet& set = inst.sets[(size_t)setIndex];

    for (const RtZone& zone : set.zones) {
        if (!zone.data) continue;
        if (key < zone.keyLow || key > zone.keyHigh || velocity < zone.velLow || velocity > zone.velHigh) continue;

        double ratio = zone.pitched ? std::exp2((double)(key - zone.rootKey) / 12.0 + (double)zone.tuneCents / 1200.0)
                                    : std::exp2((double)zone.tuneCents / 1200.0);
        double increment = ratio * (double)zone.data->sampleRate / (double)sampleRate;
        double position = elapsedSeconds * (double)sampleRate * increment;
        uint64_t frameCount = zone.data->frameCount;
        if (zone.looping && zone.loopEnd > zone.loopStart && position >= (double)zone.loopEnd) {
            double span = (double)(zone.loopEnd - zone.loopStart);
            position = (double)zone.loopStart + std::fmod(position - (double)zone.loopStart, span);
        } else if (position >= (double)frameCount) {
            continue;
        }

        Voice* v = AllocateVoice();
        v->active = true;
        v->zone = &zone;
        v->set = &set;
        v->layer = layer;
        v->instrument = instrument;
        v->state = layer * (int)sequence->instruments.size() + instrument;
        v->key = key;
        v->noteEnd = noteEnd;
        v->hasNoteEnd = true;
        v->position = position;
        v->baseIncrement = increment;
        v->velocityGain = (float)velocity / 127.0f;
        v->order = ++voiceCounter;

        float attack = std::max(set.attack, 0.001f);
        float decay = std::max(set.decay, 0.001f);
        v->attackStep = 1.0f / (attack * (float)sampleRate);
        v->decayStep = (1.0f - set.sustain) / (decay * (float)sampleRate);
        double e = elapsedSeconds;
        if (e < (double)attack) {
            v->stage = 0;
            v->envelope = (float)(e / (double)attack);
        } else if (e < (double)(attack + decay) && set.sustain < 1.0f) {
            v->stage = 1;
            v->envelope = 1.0f - (float)((e - (double)attack) / (double)decay) * (1.0f - set.sustain);
        } else {
            v->stage = 2;
            v->envelope = set.sustain;
        }
    }
}

void SequenceRenderer::StartNote(int layer, const RtEvent& event) {
    bool audible = layerTarget[(size_t)layer] > 0.0f || layerGain[(size_t)layer] > kSilence;
    int64_t end = event.tick + event.length;
    if (audible) {
        SpawnVoices(layer, event.instrument, event.key, event.velocity, end, 0.0);
        return;
    }
    size_t base = (size_t)layer * kMaxSkippedNotesPerLayer;
    size_t slot = base;
    bool found = false;
    for (size_t i = 0; i < (size_t)kMaxSkippedNotesPerLayer; i++) {
        SkippedNote& s = skipped[base + i];
        if (!s.used || (double)s.end <= tick) { slot = base + i; found = true; break; }
    }
    if (!found) {
        for (size_t i = 0; i < (size_t)kMaxSkippedNotesPerLayer; i++) {
            if (skipped[base + i].tick < skipped[slot].tick) slot = base + i;
        }
    }
    SkippedNote& s = skipped[slot];
    s.used = true;
    s.tick = event.tick;
    s.end = end;
    s.instrument = event.instrument;
    s.key = (uint8_t)event.key;
    s.velocity = (uint8_t)event.velocity;
}

void SequenceRenderer::LateStartSkipped(int layer) {
    double perFrame = TicksPerFrame();
    size_t base = (size_t)layer * kMaxSkippedNotesPerLayer;
    for (size_t i = 0; i < (size_t)kMaxSkippedNotesPerLayer; i++) {
        SkippedNote& s = skipped[base + i];
        if (!s.used) continue;
        s.used = false;
        if ((double)s.end <= tick || s.tick > tick) continue;
        double elapsedSeconds = (tick - (double)s.tick) / (perFrame * (double)sampleRate);
        SpawnVoices(layer, s.instrument, s.key, s.velocity, s.end, elapsedSeconds);
    }
}

double SequenceRenderer::NextEventTick() const {
    double best = 1e300;
    if (!ended) {
        bool looping = control->looping.load(std::memory_order_relaxed);
        int64_t endTick = (looping && sequence->loopEnd > sequence->loopStart) ? sequence->loopEnd : sequence->length;
        best = std::min(best, (double)endTick);
        for (size_t l = 0; l < sequence->layers.size(); l++) {
            const std::vector<RtEvent>& events = sequence->layers[l].events;
            if (cursors[l] < events.size()) best = std::min(best, (double)events[cursors[l]].tick);
        }
    }
    if (jumpPending) best = std::min(best, (double)jumpAt);
    if (bpmIndex + 1 < sequence->tempo.size()) best = std::min(best, (double)sequence->tempo[bpmIndex + 1].tick);
    for (const Voice& v : voices) {
        if (v.active && !v.releasing && v.hasNoteEnd) best = std::min(best, (double)v.noteEnd);
    }
    return best;
}

void SequenceRenderer::ProcessDue() {
    for (int guard = 0; guard < 64; guard++) {
        bool changed = false;
        while (bpmIndex + 1 < sequence->tempo.size() && (double)sequence->tempo[bpmIndex + 1].tick <= tick + 1e-9) bpmIndex++;
        ReleaseNoteEnds();

        if (jumpPending && tick + 1e-9 >= (double)jumpAt) {
            double over = std::max(0.0, tick - (double)jumpAt);
            jumpPending = false;
            JumpTo(jumpTarget, over);
            changed = true;
        }

        bool looping = control->looping.load(std::memory_order_relaxed);
        int64_t endTick = (looping && sequence->loopEnd > sequence->loopStart) ? sequence->loopEnd : sequence->length;
        if (!ended && tick + 1e-9 >= (double)endTick) {
            if (looping) {
                int64_t target = sequence->loopEnd > sequence->loopStart ? sequence->loopStart : 0;
                JumpTo(target, std::max(0.0, tick - (double)endTick));
                changed = true;
            } else {
                ended = true;
                ReleaseAllForJump();
            }
        }

        if (!ended) {
            for (size_t l = 0; l < sequence->layers.size(); l++) {
                const std::vector<RtEvent>& events = sequence->layers[l].events;
                while (cursors[l] < events.size() && (double)events[cursors[l]].tick <= tick + 1e-9) {
                    const RtEvent& e = events[cursors[l]++];
                    if (e.type == RtEventType::Controller) ApplyController((int)l, e, true);
                    else StartNote((int)l, e);
                }
            }
        }
        if (!changed) break;
    }
}

void SequenceRenderer::HandleRequests() {
    int64_t seek = control->seekTick.exchange(-1);
    if (seek >= 0) {
        jumpPending = false;
        JumpTo(std::clamp<int64_t>(seek, 0, sequence->length), 0.0);
    }
    int marker = control->jumpMarker.exchange(-1);
    if (marker >= 0 && (size_t)marker < sequence->markers.size()) {
        int quantize = control->quantize.load();
        jumpTarget = sequence->markers[(size_t)marker].tick;
        int64_t now = (int64_t)std::ceil(tick - 1e-9);
        jumpAt = quantize <= 0 ? now : NextBoundary(now, quantize);
        jumpPending = true;
        ended = false;
    }
}

void SequenceRenderer::MixVoice(Voice& v, float* out, int frames, float layerStart, float layerEnd, float pitchStart, float pitchEnd,
                                float fadeStart, float fadeEnd, double bendRatio, float staticGain, float panLeft, float panRight) {
    const RtZone& zone = *v.zone;
    const SampleData& data = *zone.data;
    const int channelsCount = data.channels;
    const uint64_t frameCount = data.frameCount;
    const bool looping = zone.looping && zone.loopEnd > zone.loopStart;
    const float inv = frames > 0 ? 1.0f / (float)frames : 0.0f;
    const float sustainLevel = v.set->sustain;

    for (int i = 0; i < frames; i++) {
        float t = (float)i * inv;
        float layerGainNow = layerStart + (layerEnd - layerStart) * t;
        float pitchNow = pitchStart + (pitchEnd - pitchStart) * t;
        float fadeNow = fadeStart + (fadeEnd - fadeStart) * t;

        switch (v.stage) {
            case 0:
                v.envelope += v.attackStep;
                if (v.envelope >= 1.0f) {
                    v.envelope = 1.0f;
                    v.stage = sustainLevel < 1.0f ? 1 : 2;
                }
                break;
            case 1:
                v.envelope -= v.decayStep;
                if (v.envelope <= sustainLevel) {
                    v.envelope = sustainLevel;
                    v.stage = 2;
                }
                break;
            case 3:
                v.envelope -= v.releaseStep;
                if (v.envelope <= 0.0f) {
                    v.active = false;
                    return;
                }
                break;
            default: break;
        }

        double pos = v.position;
        uint64_t index = (uint64_t)pos;
        if (index >= frameCount) {
            if (looping) {
                double span = (double)(zone.loopEnd - zone.loopStart);
                pos = (double)zone.loopStart + std::fmod(pos - (double)zone.loopStart, span);
                index = (uint64_t)pos;
            } else {
                v.active = false;
                return;
            }
        }
        float frac = (float)(pos - (double)index);
        uint64_t next = index + 1;
        if (looping && next >= zone.loopEnd) next = zone.loopStart + (next - zone.loopEnd);
        else if (next >= frameCount) next = index;

        float l, r;
        if (channelsCount == 1) {
            float a = data.frames[(size_t)index];
            float b = data.frames[(size_t)next];
            l = r = a + (b - a) * frac;
        } else {
            float al = data.frames[(size_t)(index * 2)], ar = data.frames[(size_t)(index * 2 + 1)];
            float bl = data.frames[(size_t)(next * 2)], br = data.frames[(size_t)(next * 2 + 1)];
            l = al + (bl - al) * frac;
            r = ar + (br - ar) * frac;
        }

        float g = staticGain * layerGainNow * fadeNow * v.envelope;
        out[i * 2] += l * g * panLeft;
        out[i * 2 + 1] += r * g * panRight;

        pos += v.baseIncrement * bendRatio * (double)pitchNow;
        if (looping && pos >= (double)zone.loopEnd) {
            double span = (double)(zone.loopEnd - zone.loopStart);
            pos = (double)zone.loopStart + std::fmod(pos - (double)zone.loopStart, span);
        }
        v.position = pos;
    }
}

void SequenceRenderer::RenderChunk(float* out, int frames) {
    UpdateLayerTargets();
    for (size_t l = 0; l < layerTarget.size(); l++) {
        bool audibleTarget = layerTarget[l] > 0.0f;
        if (audibleTarget && !layerAudible[l]) LateStartSkipped((int)l);
        layerAudible[l] = audibleTarget;
    }

    float fadeSeconds = control->layerFade.load(std::memory_order_relaxed);
    float perFrame = fadeSeconds > 0.0005f ? 1.0f / (fadeSeconds * (float)sampleRate) : 1.0f;
    float maxDelta = perFrame * (float)frames;
    for (size_t l = 0; l < layerGain.size(); l++) {
        float delta = std::clamp(layerTarget[l] - layerGain[l], -maxDelta, maxDelta);
        layerStartGain[l] = layerGain[l];
        layerGain[l] += delta;
        layerEndGain[l] = layerGain[l];
    }

    float pitchTarget = std::max(0.0f, control->pitch.load(std::memory_order_relaxed));
    float pitchStart = pitchSmoothed;
    pitchSmoothed += (pitchTarget - pitchSmoothed) * (1.0f - std::exp(-(float)frames / (0.01f * (float)sampleRate)));
    float pitchEnd = pitchSmoothed;

    bool stopping = control->stopRequested.load(std::memory_order_relaxed);
    bool paused = control->paused.load(std::memory_order_relaxed);
    float fadeTarget = (stopping || paused) ? 0.0f : 1.0f;
    float fadeStep = (float)frames / ((stopping ? 0.03f : 0.02f) * (float)sampleRate);
    float fadeStart = fadeGain;
    fadeGain += std::clamp(fadeTarget - fadeGain, -fadeStep, fadeStep);
    float fadeEnd = fadeGain;

    float globalPan = control->pan.load(std::memory_order_relaxed);

    for (Voice& v : voices) {
        if (!v.active) continue;
        const ChannelState& ch = channels[(size_t)v.state];
        float vol = (float)ch.volume / 127.0f;
        float expr = (float)ch.expression / 127.0f;
        float instVolume = control->instrumentVolume[(size_t)v.instrument].load(std::memory_order_relaxed);
        float staticGain = v.velocityGain * v.zone->gain * v.set->volume * vol * vol * expr * expr * instVolume;
        float pan = v.set->pan + (float)ch.pan / 64.0f + v.zone->pan + control->layerPan[(size_t)v.layer].load(std::memory_order_relaxed) + globalPan;
        pan = std::clamp(pan, -1.0f, 1.0f);
        float panLeft = std::min(1.0f, 1.0f - pan);
        float panRight = std::min(1.0f, 1.0f + pan);
        double bendRatio = std::exp2((double)ch.bend / 8192.0 * (double)v.set->bendRange / 12.0);

        MixVoice(v, out, frames, layerStartGain[(size_t)v.layer], layerEndGain[(size_t)v.layer], pitchStart, pitchEnd,
                 fadeStart, fadeEnd, bendRatio, staticGain, panLeft, panRight);
    }
}

void SequenceRenderer::Render(float* out, int frames, int rate) {
    if (!initialized) Initialize(rate);
    if (control->finished.load(std::memory_order_relaxed)) return;

    HandleRequests();

    bool stopping = control->stopRequested.load(std::memory_order_relaxed);
    bool paused = control->paused.load(std::memory_order_relaxed);
    if (stopping && fadeGain <= kSilence) {
        for (Voice& v : voices) v.active = false;
        control->finished.store(true, std::memory_order_release);
        return;
    }

    int done = 0;
    while (done < frames) {
        if (paused && fadeGain <= kSilence) break;
        ProcessDue();

        int remaining = frames - done;
        double perFrame = TicksPerFrame();
        double toNext = NextEventTick() - tick;
        int chunk = remaining;
        if (toNext <= 0.0) chunk = 1;
        else chunk = (int)std::min<double>((double)remaining, std::max(1.0, std::ceil(toNext / perFrame - 1e-9)));

        RenderChunk(out + (size_t)done * 2, chunk);
        tick += perFrame * (double)chunk;
        done += chunk;

        if (stopping && fadeGain <= kSilence) break;
    }

    if (stopping && fadeGain <= kSilence) {
        for (Voice& v : voices) v.active = false;
        control->finished.store(true, std::memory_order_release);
    } else if (ended && ActiveCount() == 0) {
        control->finished.store(true, std::memory_order_release);
    }
    control->positionTick.store(tick, std::memory_order_relaxed);
    control->activeVoices.store(ActiveCount(), std::memory_order_relaxed);
}

}
