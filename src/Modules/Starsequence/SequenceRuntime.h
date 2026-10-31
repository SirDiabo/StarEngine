#pragma once
#include "SequenceData.h"
#include "Runtime/Audio/AudioMixer.h"
#include "Runtime/Audio/WavFile.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Starsequence {

constexpr int kMaxSkippedNotesPerLayer = 256;
constexpr int kExtraDyingVoices = 8;

struct LoadedSample {
    std::shared_ptr<const SampleData> data;
    float assetGain = 1.0f;
};

using SampleLoader = std::function<bool(const std::string& assetPath, LoadedSample& out)>;

struct RtZone {
    const SampleData* data = nullptr;
    int keyLow = 0;
    int keyHigh = 127;
    int velLow = 1;
    int velHigh = 127;
    int rootKey = 60;
    bool pitched = true;
    bool looping = false;
    uint64_t loopStart = 0;
    uint64_t loopEnd = 0;
    float tuneCents = 0.0f;
    float gain = 1.0f;
    float pan = 0.0f;
};

struct RtSet {
    std::vector<RtZone> zones;
    float volume = 1.0f;
    float pan = 0.0f;
    int bendRange = 2;
    float attack = 0.002f;
    float decay = 0.1f;
    float sustain = 1.0f;
    float release = 0.1f;
};

struct RtInstrument {
    std::vector<RtSet> sets;
};

enum class RtEventType : uint8_t { Controller, NoteOn };

struct RtEvent {
    int64_t tick = 0;
    int64_t length = 0;
    RtEventType type = RtEventType::NoteOn;
    uint8_t instrument = 0;
    uint8_t controller = 0;
    int32_t value = 0;
    int32_t key = 0;
    int32_t velocity = 0;
};

struct RtLayer {
    std::vector<RtEvent> events;
};

struct RtSequence {
    int resolution = 480;
    int64_t length = 0;
    int64_t loopStart = 0;
    int64_t loopEnd = 0;
    int polyphony = 64;
    std::vector<TempoPoint> tempo;
    std::vector<Marker> markers;
    std::vector<int64_t> beats;
    std::vector<int64_t> bars;
    std::vector<RtInstrument> instruments;
    std::vector<RtLayer> layers;
    std::vector<std::shared_ptr<const SampleData>> keepAlive;
};

struct CompileReport {
    std::vector<std::string> warnings;
};

std::shared_ptr<RtSequence> CompileSequence(const Sequence& sequence, const SampleLoader& loader, CompileReport& report);

struct SequenceControl {
    SequenceControl(int layerCount, int instrumentCount);

    std::atomic<float> pitch{1.0f};
    std::atomic<float> tempo{1.0f};
    std::atomic<float> pan{0.0f};
    std::atomic<float> layerFade{0.2f};
    std::atomic<bool> paused{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> looping{false};
    std::atomic<bool> finished{false};
    std::atomic<int> quantize{2};
    std::atomic<int> jumpMarker{-1};
    std::atomic<int64_t> seekTick{-1};
    std::atomic<double> positionTick{0.0};
    std::atomic<int> activeVoices{0};

    int layers;
    int instruments;
    std::unique_ptr<std::atomic<float>[]> layerVolume;
    std::unique_ptr<std::atomic<float>[]> layerPan;
    std::unique_ptr<std::atomic<bool>[]> layerMute;
    std::unique_ptr<std::atomic<bool>[]> layerSolo;
    std::unique_ptr<std::atomic<float>[]> instrumentVolume;
    std::unique_ptr<std::atomic<int>[]> instrumentSet;
};

class SequenceRenderer : public AudioRenderer {
public:
    SequenceRenderer(std::shared_ptr<const RtSequence> sequence, std::shared_ptr<SequenceControl> control);

    void Render(float* interleavedStereo, int frames, int sampleRate) override;
    bool IsFinished() const override { return control->finished.load(std::memory_order_acquire); }

private:
    struct Voice {
        bool active = false;
        bool releasing = false;
        bool dying = false;
        bool held = false;
        bool hasNoteEnd = false;
        const RtZone* zone = nullptr;
        const RtSet* set = nullptr;
        int layer = 0;
        int instrument = 0;
        int state = 0;
        int key = 60;
        int64_t noteEnd = 0;
        double position = 0.0;
        double baseIncrement = 1.0;
        float velocityGain = 1.0f;
        int stage = 0;
        float envelope = 0.0f;
        float attackStep = 1.0f;
        float decayStep = 0.0f;
        float releaseStep = 0.0f;
        uint64_t order = 0;
    };

    struct ChannelState {
        int volume = 127;
        int expression = 127;
        int pan = 0;
        int bend = 0;
        bool sustain = false;
        double bendRatio = 1.0;
    };

    struct SkippedNote {
        int64_t tick = 0;
        int64_t end = 0;
        uint8_t instrument = 0;
        uint8_t key = 0;
        uint8_t velocity = 0;
        bool used = true;
    };

    void Initialize(int sampleRate);
    void HandleRequests();
    void ProcessDue();
    double NextEventTick() const;
    double TicksPerFrame() const;
    void RenderChunk(float* out, int frames);
    void UpdateLayerTargets();
    void StartNote(int layer, const RtEvent& event);
    void SpawnVoices(int layer, int instrument, int key, int velocity, int64_t noteEnd, double elapsedSeconds);
    Voice* AllocateVoice();
    void ReleaseVoice(Voice& voice, bool forced);
    void ReleaseNoteEnds();
    void ReleaseAllForJump();
    void JumpTo(int64_t target, double overshoot);
    void ReplayControllers(int64_t target);
    void ApplyController(int layer, const RtEvent& event, bool live);
    void LateStartSkipped(int layer);
    void ResetBpmIndex();
    int64_t NextBoundary(int64_t from, int quantize) const;
    int ActiveCount() const;
    void MixVoice(Voice& voice, float* out, int frames, float layerStart, float layerEnd, float pitchStart, float pitchEnd,
                  float fadeStart, float fadeEnd, double bendRatio, float staticGain, float panLeft, float panRight);

    std::shared_ptr<const RtSequence> sequence;
    std::shared_ptr<SequenceControl> control;

    bool initialized = false;
    int sampleRate = kMixerSampleRate;
    double tick = 0.0;
    size_t bpmIndex = 0;
    bool ended = false;
    uint64_t voiceCounter = 0;
    int polyphonyLimit = 64;

    std::vector<Voice> voices;
    std::vector<size_t> cursors;
    std::vector<ChannelState> channels;
    std::vector<SkippedNote> skipped;
    std::vector<float> layerGain;
    std::vector<float> layerTarget;
    std::vector<bool> layerAudible;
    std::vector<float> layerStartGain;
    std::vector<float> layerEndGain;

    float pitchSmoothed = 1.0f;
    float fadeGain = 1.0f;
    bool anySolo = false;

    bool jumpPending = false;
    int64_t jumpTarget = 0;
    int64_t jumpAt = 0;
};

}
