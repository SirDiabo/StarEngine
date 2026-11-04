#pragma once
#include "Core/AudioEffectTypes.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

class AudioEffect {
public:
    AudioEffect(const AudioEffectTypeInfo* typeInfo, int sampleRate);
    virtual ~AudioEffect() = default;

    const AudioEffectTypeInfo* Info() const { return info; }
    const std::string& Type() const { return info->type; }

    void SetParam(int index, float value);
    float GetParam(int index) const;
    void Process(float* interleavedStereo, int frames);
    void Reset();

protected:
    virtual void ProcessWet(float* interleavedStereo, int frames) = 0;
    virtual void ResetState() = 0;
    virtual void Update() = 0;

    float P(int index) const { return current[index]; }

    int rate;

private:
    static constexpr int kChunkFrames = 512;

    const AudioEffectTypeInfo* info;
    std::atomic<float> target[AudioEffectTypes::kMaxParams];
    float current[AudioEffectTypes::kMaxParams];
    bool primed = false;
    std::vector<float> dry;
};

struct AudioEffectChain {
    std::vector<std::shared_ptr<AudioEffect>> effects;
    std::vector<std::string> names;
};

std::shared_ptr<AudioEffect> CreateAudioEffect(const std::string& type, int sampleRate);
