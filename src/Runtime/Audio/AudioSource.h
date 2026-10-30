#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class AudioParamType { Bool, Float, Int, Enum, Asset, Trigger };

struct AudioParamInfo {
    std::string path;
    std::string group;
    AudioParamType type = AudioParamType::Float;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::vector<std::string> options;
    std::string assetFilter;
};

struct AudioParamValue {
    float number = 0.0f;
    std::string text;

    static AudioParamValue FromNumber(float value) { AudioParamValue v; v.number = value; return v; }
    static AudioParamValue FromText(const std::string& value) { AudioParamValue v; v.text = value; return v; }
};

class AudioSource {
public:
    virtual ~AudioSource() = default;

    virtual void Play() = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual bool IsPlaying() const = 0;
    virtual bool IsPaused() const = 0;

    virtual void SetVolume(float volume) = 0;
    virtual void SetPitch(float pitch) = 0;
    virtual void SetLooping(bool looping) = 0;
    virtual void SetBus(int bus) = 0;
    virtual void SetPan(float pan) { (void)pan; }
    virtual std::string DefaultBusName() const { return std::string(); }

    virtual void SetTempo(float tempo) { (void)tempo; }
    virtual bool SupportsTempo() const { return false; }

    virtual void Seek(double seconds) { (void)seconds; }
    virtual double GetPosition() const { return 0.0; }
    virtual double GetDuration() const { return 0.0; }

    virtual void GetParameters(std::vector<AudioParamInfo>& out) const { (void)out; }
    virtual bool SetParameter(const std::string& path, const AudioParamValue& value) { (void)path; (void)value; return false; }

    virtual void Tick(float deltaTime) { (void)deltaTime; }
};

struct AudioSourceType {
    std::string extension;
    std::string displayName;
    std::string moduleName;
    std::function<std::unique_ptr<AudioSource>(const std::string& assetPath)> create;
    std::function<void(const std::string& assetPath, std::vector<AudioParamInfo>& out)> describeParameters;
};

class AudioSourceRegistry {
public:
    static AudioSourceRegistry& Get();

    void Register(const AudioSourceType& type);
    const AudioSourceType* FindForPath(const std::string& assetPath) const;
    std::unique_ptr<AudioSource> Create(const std::string& assetPath) const;
    bool DescribeParameters(const std::string& assetPath, std::vector<AudioParamInfo>& out) const;
    std::vector<std::string> GetExtensions() const;

private:
    std::vector<AudioSourceType> types;
};

struct AudioSourceRegistrar {
    explicit AudioSourceRegistrar(const AudioSourceType& type) { AudioSourceRegistry::Get().Register(type); }
};
