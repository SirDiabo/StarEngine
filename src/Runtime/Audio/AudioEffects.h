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
    virtual float TailSeconds() const { return 0.15f; }

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

    static std::shared_ptr<const AudioEffectChain> Merge(const std::shared_ptr<const AudioEffectChain>& first,
                                                         const std::shared_ptr<const AudioEffectChain>& second) {
        if (!first || first->effects.empty()) return second;
        if (!second || second->effects.empty()) return first;
        auto merged = std::make_shared<AudioEffectChain>();
        merged->effects = first->effects;
        merged->names = first->names;
        merged->effects.insert(merged->effects.end(), second->effects.begin(), second->effects.end());
        merged->names.insert(merged->names.end(), second->names.begin(), second->names.end());
        return merged;
    }

    float TailSeconds() const {
        float total = 0.0f;
        for (const auto& effect : effects) total += effect->TailSeconds();
        return total > 12.0f ? 12.0f : total;
    }
};

std::shared_ptr<AudioEffect> CreateAudioEffect(const std::string& type, int sampleRate);
