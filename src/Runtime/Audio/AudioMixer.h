#pragma once
#include "AudioStreamFeed.h"
#include "WavFile.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

constexpr int kMixerSampleRate = 44100;
constexpr int kMaxMixerBuses = 32;
constexpr int kMaxMixerVoices = 256;

struct VoiceControl {
    std::atomic<float> volume{1.0f};
    std::atomic<float> pan{0.0f};
    std::atomic<float> pitch{1.0f};
    std::atomic<bool> paused{false};
    std::atomic<bool> looping{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> finished{false};
    std::atomic<int> bus{0};
    std::atomic<int64_t> seekFrame{-1};
    std::atomic<uint64_t> positionFrames{0};
    int sourceRate = kMixerSampleRate;
    uint64_t totalFrames = 0;
};

struct VoiceStart {
    float volume = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    bool looping = false;
    bool paused = false;
    int bus = 0;
    uint64_t loopStart = 0;
    uint64_t loopEnd = 0;
};

class AudioRenderer {
public:
    virtual ~AudioRenderer() = default;
    virtual void Render(float* interleavedStereo, int frames, int sampleRate) = 0;
    virtual bool IsFinished() const { return false; }

    std::atomic<float> gain{1.0f};
    std::atomic<int> bus{0};
};

template <typename T, size_t N>
class SpscQueue {
public:
    bool Push(T value) {
        std::lock_guard<std::mutex> guard(producerLock);
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next = (t + 1) % N;
        if (next == head.load(std::memory_order_acquire)) return false;
        slots[t] = std::move(value);
        tail.store(next, std::memory_order_release);
        return true;
    }

    bool Pop(T& out) {
        size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        out = std::move(slots[h]);
        slots[h] = T();
        head.store((h + 1) % N, std::memory_order_release);
        return true;
    }

private:
    T slots[N];
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::mutex producerLock;
};

class AudioMixer {
public:
    static AudioMixer& Get();

    bool Start();
    void Shutdown();
    bool IsRunning() const { return running.load(); }

    void Render(float* interleavedStereo, int frames);

    std::shared_ptr<VoiceControl> PlayMemory(const std::shared_ptr<const SampleData>& data, const VoiceStart& start);
    std::shared_ptr<VoiceControl> PlayStream(const std::shared_ptr<StreamFeed>& feed, const VoiceStart& start);

    void AddRenderer(const std::shared_ptr<AudioRenderer>& renderer);
    void RemoveRenderer(const std::shared_ptr<AudioRenderer>& renderer);

    void SetBusGain(int bus, float gain);
    float GetBusGain(int bus) const;

    void StopAllVoices();
    int ActiveVoiceCount() const { return activeVoices.load(); }

private:
    struct Voice {
        std::shared_ptr<VoiceControl> control;
        std::shared_ptr<const SampleData> data;
        std::shared_ptr<StreamFeed> feed;
        double position = 0.0;
        float gain = 0.0f;
        float pitch = 1.0f;
        bool pitchInitialized = false;
        uint64_t loopStart = 0;
        uint64_t loopEnd = 0;
        int sourceRate = kMixerSampleRate;
    };

    struct RendererCommand {
        bool add = true;
        std::shared_ptr<AudioRenderer> renderer;
    };

    struct ActiveRenderer {
        std::shared_ptr<AudioRenderer> renderer;
        float lastGain = 1.0f;
    };

    static void StreamCallback(void* buffer, unsigned int frames);

    void DrainQueues();
    bool MixVoice(Voice& voice, float* out, int frames);

    std::atomic<bool> running{false};
    std::atomic<bool> stopAll{false};
    std::atomic<int> activeVoices{0};
    std::atomic<float> busGains[kMaxMixerBuses];
    bool streamCreated = false;

    SpscQueue<std::shared_ptr<Voice>, 512> voiceQueue;
    SpscQueue<RendererCommand, 64> rendererQueue;
    std::vector<std::shared_ptr<Voice>> voices;
    std::vector<ActiveRenderer> renderers;
    std::vector<float> rendererScratch;

    struct StreamHandle;
    StreamHandle* stream = nullptr;

    AudioMixer();
    ~AudioMixer();
};
