#include "AudioMixer.h"
#include "AudioEffects.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr float kGainSmoothing = 0.0075f;
constexpr float kPitchSmoothing = 0.0025f;
constexpr float kSilence = 0.0001f;

}

struct AudioMixer::StreamHandle {
    AudioStream stream{};
};

AudioMixer::AudioMixer() {
    for (int i = 0; i < kMaxMixerBuses; i++) {
        busGains[i].store(1.0f);
        busLastGain[i] = 1.0f;
        busGainPrimed[i] = false;
        chainActive[i] = false;
        blockBusFactor[i] = 1.0f;
        blockBusDest[i] = nullptr;
    }
    voices.reserve(kMaxMixerVoices);
    renderers.reserve(32);
}

AudioMixer::~AudioMixer() {
    delete stream;
}

AudioMixer& AudioMixer::Get() {
    static AudioMixer instance;
    return instance;
}

bool AudioMixer::Start() {
    if (streamCreated) return true;
    SetAudioStreamBufferSizeDefault(1024);
    stream = new StreamHandle();
    stream->stream = LoadAudioStream(kMixerSampleRate, 32, 2);
    if (stream->stream.buffer == nullptr) {
        delete stream;
        stream = nullptr;
        return false;
    }
    SetAudioStreamCallback(stream->stream, &AudioMixer::StreamCallback);
    running.store(true);
    PlayAudioStream(stream->stream);
    streamCreated = true;
    return true;
}

void AudioMixer::Shutdown() {
    running.store(false);
    StreamService::Get().Shutdown();
    if (stream) {
        StopAudioStream(stream->stream);
        UnloadAudioStream(stream->stream);
        delete stream;
        stream = nullptr;
    }
    streamCreated = false;
    voices.clear();
    renderers.clear();
    for (int i = 0; i < kMaxMixerBuses; i++) busChains[i].reset();
    activeVoices.store(0);
}

void AudioMixer::StreamCallback(void* buffer, unsigned int frames) {
    Get().Render((float*)buffer, (int)frames);
}

std::shared_ptr<VoiceControl> AudioMixer::PlayMemory(const std::shared_ptr<const SampleData>& data, const VoiceStart& start) {
    if (!data || data->frameCount == 0) return nullptr;

    auto voice = std::make_shared<Voice>();
    voice->epoch = stopEpoch.load();
    voice->control = std::make_shared<VoiceControl>();
    voice->data = data;
    voice->sourceRate = data->sampleRate;
    voice->loopStart = start.loopEnd > start.loopStart ? start.loopStart : 0;
    voice->loopEnd = start.loopEnd > start.loopStart ? std::min<uint64_t>(start.loopEnd, data->frameCount) : data->frameCount;

    VoiceControl& c = *voice->control;
    c.volume.store(start.volume);
    c.pan.store(start.pan);
    c.pitch.store(start.pitch);
    c.looping.store(start.looping);
    c.paused.store(start.paused);
    c.bus.store(start.bus);
    c.sourceRate = data->sampleRate;
    c.totalFrames = data->frameCount;
    voice->chain = start.chain;

    if (!voiceQueue.Push(voice)) return nullptr;
    return voice->control;
}

std::shared_ptr<VoiceControl> AudioMixer::PlayStream(const std::shared_ptr<StreamFeed>& feed, const VoiceStart& start) {
    if (!feed || !feed->IsOpen()) return nullptr;

    auto voice = std::make_shared<Voice>();
    voice->epoch = stopEpoch.load();
    voice->control = std::make_shared<VoiceControl>();
    voice->feed = feed;
    voice->sourceRate = feed->Info().sampleRate;

    VoiceControl& c = *voice->control;
    c.volume.store(start.volume);
    c.pan.store(start.pan);
    c.pitch.store(start.pitch);
    c.looping.store(start.looping);
    c.paused.store(start.paused);
    c.bus.store(start.bus);
    c.sourceRate = feed->Info().sampleRate;
    c.totalFrames = feed->Info().frameCount;
    voice->chain = start.chain;

    feed->SetLooping(start.looping);
    StreamService::Get().Add(feed);

    if (!voiceQueue.Push(voice)) return nullptr;
    return voice->control;
}

void AudioMixer::AddRenderer(const std::shared_ptr<AudioRenderer>& renderer, const std::shared_ptr<const AudioEffectChain>& chain) {
    if (!renderer) return;
    rendererQueue.Push({ true, renderer, stopEpoch.load(), chain });
}

void AudioMixer::SetVoiceChain(const std::shared_ptr<VoiceControl>& control, const std::shared_ptr<const AudioEffectChain>& chain) {
    if (!control) return;
    voiceChainQueue.Push({ control, chain });
}

void AudioMixer::SetRendererChain(const std::shared_ptr<AudioRenderer>& renderer, const std::shared_ptr<const AudioEffectChain>& chain) {
    if (!renderer) return;
    rendererChainQueue.Push({ renderer, chain });
}

void AudioMixer::RemoveRenderer(const std::shared_ptr<AudioRenderer>& renderer) {
    if (!renderer) return;
    rendererQueue.Push({ false, renderer });
}

void AudioMixer::SetBusGain(int bus, float gain) {
    if (bus < 0 || bus >= kMaxMixerBuses) return;
    busGains[bus].store(std::max(0.0f, gain));
}

float AudioMixer::GetBusGain(int bus) const {
    if (bus < 0 || bus >= kMaxMixerBuses) return 1.0f;
    return busGains[bus].load();
}

void AudioMixer::SetBusChain(int bus, const std::shared_ptr<const AudioEffectChain>& chain) {
    if (bus < 0 || bus >= kMaxMixerBuses) return;
    ChainCommand command;
    command.bus = bus;
    command.chain = chain;
    if (chain && !chain->effects.empty() && bus > 0) command.scratch.assign((size_t)kBusScratchFrames * 2, 0.0f);
    chainQueue.Push(std::move(command));
}

void AudioMixer::StopAllVoices() {
    stopEpoch.fetch_add(1);
}

void AudioMixer::DrainQueues() {
    std::shared_ptr<Voice> incoming;
    while (voiceQueue.Pop(incoming)) {
        if (voices.size() >= (size_t)kMaxMixerVoices) {
            incoming->control->finished.store(true);
            continue;
        }
        voices.push_back(std::move(incoming));
    }

    VoiceChainCommand voiceChainCommand;
    while (voiceChainQueue.Pop(voiceChainCommand)) {
        for (auto& voice : voices) {
            if (voice->control == voiceChainCommand.control) { voice->chain = voiceChainCommand.chain; break; }
        }
    }

    ChainCommand chainCommand;
    while (chainQueue.Pop(chainCommand)) {
        busChains[chainCommand.bus] = std::move(chainCommand.chain);
        if (!chainCommand.scratch.empty()) busScratch[chainCommand.bus] = std::move(chainCommand.scratch);
        busGainPrimed[chainCommand.bus] = false;
    }

    RendererCommand command;
    while (rendererQueue.Pop(command)) {
        if (command.add) {
            ActiveRenderer entry;
            entry.renderer = command.renderer;
            entry.lastGain = command.renderer->gain.load();
            entry.epoch = command.epoch;
            entry.chain = command.chain;
            renderers.push_back(std::move(entry));
        } else {
            renderers.erase(std::remove_if(renderers.begin(), renderers.end(),
                [&](const ActiveRenderer& r) { return r.renderer == command.renderer; }), renderers.end());
        }
    }
}

void AudioMixer::PrepareBlock(float* out, int frames) {
    masterChainActive = busChains[0] && !busChains[0]->effects.empty();
    const float masterGain = busGains[0].load(std::memory_order_relaxed);
    const float masterFold = masterChainActive ? 1.0f : masterGain;

    blockBusFactor[0] = masterFold;
    blockBusDest[0] = out;
    chainActive[0] = false;

    for (int b = 1; b < kMaxMixerBuses; b++) {
        chainActive[b] = busChains[b] && !busChains[b]->effects.empty();
        if (chainActive[b]) {
            if (busScratch[b].size() < (size_t)frames * 2) busScratch[b].resize((size_t)frames * 2);
            std::fill(busScratch[b].begin(), busScratch[b].begin() + (size_t)frames * 2, 0.0f);
            blockBusFactor[b] = 1.0f;
            blockBusDest[b] = busScratch[b].data();
        } else {
            blockBusFactor[b] = busGains[b].load(std::memory_order_relaxed) * masterFold;
            blockBusDest[b] = out;
        }
    }
}

bool AudioMixer::MixVoice(Voice& voice, int frames, float* overrideDest) {
    VoiceControl& c = *voice.control;
    float* out = overrideDest ? overrideDest : blockBusDest[std::clamp(c.bus.load(std::memory_order_relaxed), 0, kMaxMixerBuses - 1)];
    const bool hasStream = voice.feed != nullptr;
    const SampleData* data = voice.data.get();

    if (c.seekFrame.load(std::memory_order_relaxed) >= 0) {
        int64_t target = c.seekFrame.exchange(-1);
        if (hasStream) {
            voice.feed->RequestSeek((uint64_t)target);
        } else if (data) {
            voice.position = (double)std::min<uint64_t>((uint64_t)target, data->frameCount ? data->frameCount - 1 : 0);
        }
    }

    const bool looping = c.looping.load(std::memory_order_relaxed);
    if (hasStream) voice.feed->SetLooping(looping);

    const bool stopping = c.stopRequested.load(std::memory_order_relaxed);
    const bool paused = c.paused.load(std::memory_order_relaxed);
    const float bus = overrideDest ? 1.0f : blockBusFactor[std::clamp(c.bus.load(std::memory_order_relaxed), 0, kMaxMixerBuses - 1)];

    const float pan = std::clamp(c.pan.load(std::memory_order_relaxed), -1.0f, 1.0f);
    const float panLeft = std::min(1.0f, 1.0f - pan);
    const float panRight = std::min(1.0f, 1.0f + pan);

    const float targetPitch = std::max(0.0f, c.pitch.load(std::memory_order_relaxed));
    if (!voice.pitchInitialized) { voice.pitch = targetPitch; voice.pitchInitialized = true; }
    const float targetGain = (stopping || paused) ? 0.0f : c.volume.load(std::memory_order_relaxed) * bus;

    if (paused && !stopping && voice.gain <= kSilence) {
        voice.gain = 0.0f;
        return true;
    }

    const double rateRatio = (double)voice.sourceRate / (double)kMixerSampleRate;
    const int channels = hasStream ? 2 : data->channels;
    const uint64_t frameCount = hasStream ? voice.feed->Info().frameCount : data->frameCount;

    for (int i = 0; i < frames; i++) {
        voice.gain += (targetGain - voice.gain) * kGainSmoothing;
        voice.pitch += (targetPitch - voice.pitch) * kPitchSmoothing;

        if (stopping && voice.gain <= kSilence) {
            c.finished.store(true, std::memory_order_release);
            return false;
        }

        float l = 0.0f, r = 0.0f;

        if (hasStream) {
            uint64_t avail = voice.feed->Available();
            if (avail < 2) {
                if (voice.feed->AtEnd() && avail == 0) {
                    c.finished.store(true, std::memory_order_release);
                    return false;
                }
                if (voice.feed->AtEnd() && avail == 1) {
                    voice.feed->Peek(0, l, r);
                    voice.feed->Advance(1);
                    voice.position = 0.0;
                    out[i * 2] += l * voice.gain * panLeft;
                    out[i * 2 + 1] += r * voice.gain * panRight;
                }
                continue;
            }
            float l0, r0, l1, r1;
            voice.feed->Peek(0, l0, r0);
            voice.feed->Peek(1, l1, r1);
            float t = std::min(1.0f, (float)voice.position);
            l = l0 + (l1 - l0) * t;
            r = r0 + (r1 - r0) * t;

            voice.position += (double)voice.pitch * rateRatio;
            uint64_t whole = (uint64_t)voice.position;
            if (whole > 0) {
                uint64_t take = std::min<uint64_t>(whole, avail);
                voice.feed->Advance(take);
                voice.position -= (double)take;
            }
        } else {
            double pos = voice.position;
            uint64_t index = (uint64_t)pos;
            if (index >= frameCount) {
                if (looping) {
                    uint64_t span = voice.loopEnd - voice.loopStart;
                    pos = (double)voice.loopStart + std::fmod(pos - (double)voice.loopStart, (double)span);
                    index = (uint64_t)pos;
                } else {
                    c.finished.store(true, std::memory_order_release);
                    return false;
                }
            }
            double frac = pos - (double)index;
            uint64_t next = index + 1;
            if (looping && next >= voice.loopEnd) next = voice.loopStart + (next - voice.loopEnd);
            else if (next >= frameCount) next = index;

            if (channels == 1) {
                float a = data->frames[(size_t)index];
                float b = data->frames[(size_t)next];
                l = r = a + (b - a) * (float)frac;
            } else {
                float al = data->frames[(size_t)(index * 2)], ar = data->frames[(size_t)(index * 2 + 1)];
                float bl = data->frames[(size_t)(next * 2)], br = data->frames[(size_t)(next * 2 + 1)];
                l = al + (bl - al) * (float)frac;
                r = ar + (br - ar) * (float)frac;
            }

            pos += (double)voice.pitch * rateRatio;
            if (looping && pos >= (double)voice.loopEnd) {
                double span = (double)(voice.loopEnd - voice.loopStart);
                pos = (double)voice.loopStart + std::fmod(pos - (double)voice.loopStart, span);
            }
            voice.position = pos;
        }

        out[i * 2] += l * voice.gain * panLeft;
        out[i * 2 + 1] += r * voice.gain * panRight;
    }

    if (hasStream) c.positionFrames.store(voice.feed->PositionFrames(), std::memory_order_relaxed);
    else c.positionFrames.store((uint64_t)voice.position, std::memory_order_relaxed);
    return true;
}

bool AudioMixer::MixChainedVoice(Voice& voice, int frames) {
    if (voiceScratch.size() < (size_t)frames * 2) voiceScratch.resize((size_t)frames * 2);
    float* scratch = voiceScratch.data();
    std::fill(scratch, scratch + (size_t)frames * 2, 0.0f);

    if (!voice.draining) {
        if (voice.control->finished.load(std::memory_order_relaxed)) voice.draining = true;
        else if (!MixVoice(voice, frames, scratch)) voice.draining = true;
    }
    if (voice.draining && voice.killTail) return false;

    const bool hasChain = voice.chain && !voice.chain->effects.empty();
    if (!hasChain && voice.draining) return false;

    float peak = 0.0f;
    if (hasChain) {
        for (const auto& effect : voice.chain->effects) effect->Process(scratch, frames);
        for (int i = 0; i < frames * 2; i++) peak = std::max(peak, std::fabs(scratch[i]));
    }

    const int busIndex = std::clamp(voice.control->bus.load(std::memory_order_relaxed), 0, kMaxMixerBuses - 1);
    float* dest = blockBusDest[busIndex];
    const float target = blockBusFactor[busIndex];
    if (!voice.busPrimed) { voice.lastBusFactor = target; voice.busPrimed = true; }
    const float step = (target - voice.lastBusFactor) / (float)frames;
    float g = voice.lastBusFactor;
    for (int f = 0; f < frames; f++) {
        g += step;
        dest[f * 2] += scratch[f * 2] * g;
        dest[f * 2 + 1] += scratch[f * 2 + 1] * g;
    }
    voice.lastBusFactor = target;

    if (voice.draining) {
        if (voice.tailFrames < 0) voice.tailFrames = (int)(voice.chain->TailSeconds() * (float)kMixerSampleRate);
        voice.silentBlocks = peak < 1e-4f ? voice.silentBlocks + 1 : 0;
        voice.drainFrames += frames;
        if ((voice.silentBlocks >= 4 && voice.drainFrames >= voice.tailFrames) || voice.drainFrames > kMixerSampleRate * 12) return false;
    }
    return true;
}

void AudioMixer::Render(float* out, int frames) {
    std::fill(out, out + (size_t)frames * 2, 0.0f);
    DrainQueues();

    PrepareBlock(out, frames);

    uint64_t currentStopEpoch = stopEpoch.load();
    if (currentStopEpoch != handledStopEpoch) {
        handledStopEpoch = currentStopEpoch;
        for (auto& voice : voices) {
            if (voice->epoch < currentStopEpoch) {
                voice->control->stopRequested.store(true);
                voice->killTail = true;
            }
        }
        for (auto& r : renderers) {
            if (r.epoch < currentStopEpoch) {
                r.renderer->gain.store(0.0f);
                r.killTail = true;
            }
        }
    }

    for (size_t i = 0; i < voices.size();) {
        Voice& voice = *voices[i];
        const bool chained = voice.draining || (voice.chain && !voice.chain->effects.empty());
        const bool done = chained ? !MixChainedVoice(voice, frames)
                                  : (voice.control->finished.load(std::memory_order_relaxed) || !MixVoice(voice, frames));
        if (done) {
            voice.control->finished.store(true, std::memory_order_release);
            voices[i] = std::move(voices.back());
            voices.pop_back();
        } else {
            i++;
        }
    }
    activeVoices.store((int)voices.size(), std::memory_order_relaxed);

    RendererChainCommand rendererChainCommand;
    while (rendererChainQueue.Pop(rendererChainCommand)) {
        for (auto& entry : renderers) {
            if (entry.renderer == rendererChainCommand.renderer) { entry.chain = rendererChainCommand.chain; break; }
        }
    }

    if (!renderers.empty()) {
        if (rendererScratch.size() < (size_t)frames * 2) rendererScratch.resize((size_t)frames * 2);
        for (size_t i = 0; i < renderers.size();) {
            ActiveRenderer& entry = renderers[i];
            const bool hasChain = entry.chain && !entry.chain->effects.empty();
            if (!entry.draining && entry.renderer->IsFinished()) {
                if (hasChain && !entry.killTail) {
                    entry.draining = true;
                } else {
                    renderers.erase(renderers.begin() + (long)i);
                    continue;
                }
            }
            if (entry.draining && (entry.killTail || !hasChain)) {
                renderers.erase(renderers.begin() + (long)i);
                continue;
            }
            std::fill(rendererScratch.begin(), rendererScratch.begin() + (size_t)frames * 2, 0.0f);
            if (!entry.draining) entry.renderer->Render(rendererScratch.data(), frames, kMixerSampleRate);

            float peak = 0.0f;
            if (hasChain) {
                for (const auto& effect : entry.chain->effects) effect->Process(rendererScratch.data(), frames);
                for (int f = 0; f < frames * 2; f++) peak = std::max(peak, std::fabs(rendererScratch[(size_t)f]));
            }

            int bus = std::clamp(entry.renderer->bus.load(std::memory_order_relaxed), 0, kMaxMixerBuses - 1);
            float* dest = blockBusDest[bus];
            float target = entry.renderer->gain.load(std::memory_order_relaxed) * blockBusFactor[bus];
            float step = (target - entry.lastGain) / (float)frames;
            float g = entry.lastGain;
            for (int f = 0; f < frames; f++) {
                g += step;
                dest[f * 2] += rendererScratch[(size_t)f * 2] * g;
                dest[f * 2 + 1] += rendererScratch[(size_t)f * 2 + 1] * g;
            }
            entry.lastGain = target;
            if (entry.draining) {
                if (entry.tailFrames < 0) entry.tailFrames = (int)(entry.chain->TailSeconds() * (float)kMixerSampleRate);
                entry.silentBlocks = peak < 1e-4f ? entry.silentBlocks + 1 : 0;
                entry.drainFrames += frames;
                if ((entry.silentBlocks >= 4 && entry.drainFrames >= entry.tailFrames) || entry.drainFrames > kMixerSampleRate * 12) {
                    renderers.erase(renderers.begin() + (long)i);
                    continue;
                }
            }
            i++;
        }
    }

    const float masterGain = busGains[0].load(std::memory_order_relaxed);
    const float masterFold = masterChainActive ? 1.0f : masterGain;

    for (int b = 1; b < kMaxMixerBuses; b++) {
        if (!chainActive[b]) { busGainPrimed[b] = false; continue; }
        float* buffer = busScratch[b].data();
        for (const auto& effect : busChains[b]->effects) effect->Process(buffer, frames);

        float target = busGains[b].load(std::memory_order_relaxed) * masterFold;
        if (!busGainPrimed[b]) { busLastGain[b] = target; busGainPrimed[b] = true; }
        float step = (target - busLastGain[b]) / (float)frames;
        float g = busLastGain[b];
        for (int f = 0; f < frames; f++) {
            g += step;
            out[f * 2] += buffer[f * 2] * g;
            out[f * 2 + 1] += buffer[f * 2 + 1] * g;
        }
        busLastGain[b] = target;
    }

    if (masterChainActive) {
        for (const auto& effect : busChains[0]->effects) effect->Process(out, frames);
        float step = (masterGain - masterLastGain) / (float)frames;
        float g = masterLastGain;
        for (int f = 0; f < frames; f++) {
            g += step;
            out[f * 2] *= g;
            out[f * 2 + 1] *= g;
        }
    }
    masterLastGain = masterGain;

    for (int i = 0; i < frames * 2; i++) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}
