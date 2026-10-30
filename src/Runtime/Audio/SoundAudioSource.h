#pragma once
#include "AudioMixer.h"
#include "AudioSource.h"
#include "WavFile.h"
#include <memory>
#include <string>

enum class SoundLoadMode { Auto, Memory, Stream };

const char* SoundLoadModeName(SoundLoadMode mode);
SoundLoadMode ParseSoundLoadMode(const std::string& text);

class SoundAudioSource : public AudioSource {
public:
    static constexpr double kAutoStreamSeconds = 10.0;

    explicit SoundAudioSource(const std::string& assetPath);
    ~SoundAudioSource() override;

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

    void Seek(double seconds) override;
    double GetPosition() const override;
    double GetDuration() const override;

    bool IsStreaming() const { return streaming; }

private:
    bool Prepare();

    std::string assetPath;
    bool prepared = false;
    bool failed = false;
    bool streaming = false;
    std::string resolvedPath;
    WavInfo info;
    std::shared_ptr<const SampleData> sample;

    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool looping = false;
    int bus = 0;

    std::shared_ptr<VoiceControl> voice;
};
