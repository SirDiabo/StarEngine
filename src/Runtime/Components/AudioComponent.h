#pragma once
#include "SceneComponent.h"
#include "Runtime/Audio/AudioSource.h"
#include <memory>
#include <string>

class AudioComponent : public SceneComponent {
public:
    std::string soundPath;
    bool autoPlay = false;
    bool looping = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float tempo = 1.0f;
    float pan = 0.0f;
    std::string bus;

    AudioComponent();
    ~AudioComponent() override;

    const char* GetClassName() const override { return "AudioComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;

    void Play();
    void Stop();
    void Pause();
    void Resume();

    bool IsPlaying() const;
    AudioSource* GetSource() { return source.get(); }
    AudioSource* EnsureSource();
    void SetParameter(const std::string& path, const AudioParamValue& value);

private:
    void SyncSource();

    std::unique_ptr<AudioSource> source;
    std::string sourcePath;
    std::string appliedBusKey;
    bool wantsPlaying = false;
    bool paused = false;
};

