#pragma once
#include "SequenceData.h"
#include "SequenceRuntime.h"
#include "Runtime/Audio/AudioSource.h"
#include <memory>
#include <string>
#include <vector>

namespace Starsequence {

bool LoadStarsoundSample(const std::string& assetPath, LoadedSample& out);
void DescribeSequenceParameters(const Sequence& sequence, std::vector<AudioParamInfo>& out);

class SequenceAudioSource : public AudioSource {
public:
    explicit SequenceAudioSource(const std::string& assetPath, SampleLoader loader = LoadStarsoundSample);
    explicit SequenceAudioSource(const Sequence& source, SampleLoader loader = LoadStarsoundSample);
    ~SequenceAudioSource() override;

    void Play() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    bool IsPlaying() const override;
    bool IsPaused() const override;

    void SetVolume(float value) override;
    void SetPitch(float value) override;
    void SetLooping(bool value) override;
    void SetBus(int value) override;
    void SetPan(float value) override;
    void SetTempo(float value) override;
    bool SupportsTempo() const override { return true; }
    std::string DefaultBusName() const override;

    void Seek(double seconds) override;
    double GetPosition() const override;
    double GetPositionTick() const;
    double GetDuration() const override;

    void GetParameters(std::vector<AudioParamInfo>& out) const override;
    bool SetParameter(const std::string& path, const AudioParamValue& value) override;
    void SetEffectChain(const std::shared_ptr<const AudioEffectChain>& chain) override;
    bool GetParameter(const std::string& path, AudioParamValue& out) const override;

    bool IsValid() const { return valid; }
    const Sequence& GetSequence() const { return sequence; }
    int ActiveVoices() const { return control ? control->activeVoices.load() : 0; }

private:
    bool Prepare();
    void InitControl();
    void CopyControl(const SequenceControl& from, SequenceControl& to) const;

    std::string assetPath;
    SampleLoader loader;
    Sequence sequence;
    bool valid = false;
    bool prepared = false;
    bool started = false;
    std::shared_ptr<const RtSequence> compiled;
    std::shared_ptr<SequenceControl> control;
    std::shared_ptr<SequenceRenderer> renderer;
    std::shared_ptr<const AudioEffectChain> effectChain;
    float volume = 1.0f;
    int bus = 0;
};

}
