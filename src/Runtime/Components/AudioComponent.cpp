#include "AudioComponent.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/SafeParse.h"
#include "Runtime/AudioCore.h"
#include "Core/AudioBusRegistry.h"
#include "Runtime/AudioCore.h"
#include <cstdlib>

AudioComponent::AudioComponent() = default;
AudioComponent::~AudioComponent() = default;

void AudioComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Sound Path", &soundPath);
    outProperties.AddBool("Auto Play", &autoPlay);
    outProperties.AddBool("Looping", &looping);
    outProperties.AddFloat("Volume", &volume);
    outProperties.AddFloat("Pitch", &pitch);
    outProperties.AddFloat("Tempo", &tempo);
    outProperties.AddString("Bus", &bus);
}

AudioSource* AudioComponent::EnsureSource() {
    if (soundPath.empty()) {
        source.reset();
        sourcePath.clear();
        return nullptr;
    }
    if (!source || sourcePath != soundPath) {
        source = AudioSourceRegistry::Get().Create(soundPath);
        sourcePath = soundPath;
        appliedBusKey.clear();
    }
    return source.get();
}

void AudioComponent::SyncSource() {
    if (!source) return;
    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
    if (source->SupportsTempo()) source->SetTempo(tempo);

    std::string key = sourcePath + "|" + bus;
    if (key != appliedBusKey) {
        appliedBusKey = key;
        int index = AudioCore::Get().ResolveBus(source->DefaultBusName(), bus);
        source->SetBus(index < 0 ? 0 : index);
    }
}

void AudioComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
    if (!source) return;
    SyncSource();
    source->Tick(deltaTime);
}

bool AudioComponent::IsPlaying() const {
    return source && source->IsPlaying();
}

void AudioComponent::Play() {
    wantsPlaying = true;
    paused = false;
    if (!EnsureSource()) return;
    SyncSource();
    source->Play();
}

void AudioComponent::Stop() {
    wantsPlaying = false;
    paused = false;
    if (source) source->Stop();
}

void AudioComponent::Pause() {
    if (!wantsPlaying || paused) return;
    paused = true;
    if (source) source->Pause();
}

void AudioComponent::Resume() {
    if (!wantsPlaying || !paused) return;
    paused = false;
    if (source) source->Resume();
}

void AudioComponent::SetParameter(const std::string& path, const AudioParamValue& value) {
    if (!EnsureSource()) return;
    source->SetParameter(path, value);
}

namespace {

ComponentFieldDesc BusField() {
    ComponentFieldDesc d = ComponentField::Enm("Bus", "Bus", {}, AudioBusRegistry::AssetDefaultLabel(),
        [](Component& c, const ComponentDecl& decl) {
            std::string value = decl.Get("Bus", AudioBusRegistry::AssetDefaultLabel());
            static_cast<AudioComponent&>(c).bus = value == AudioBusRegistry::AssetDefaultLabel() ? std::string() : value;
        },
        [](const Component& c) -> std::string {
            const std::string& value = static_cast<const AudioComponent&>(c).bus;
            return value.empty() ? std::string(AudioBusRegistry::AssetDefaultLabel()) : value;
        });
    d.enumProvider = []() {
        std::vector<std::string> options = { AudioBusRegistry::AssetDefaultLabel() };
        for (const std::string& name : AudioBusRegistry::Get().Names()) options.push_back(name);
        return options;
    };
    return d;
}

ComponentRegistrar registrar_AudioComponent({
    "AudioComponent",
    []() -> Component* { return new AudioComponent(); },
    {
        ComponentField::Asset("Sound", "Sound Path", "@audio",
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).soundPath = d.Get("Sound"); }, [](const Component& c) { return static_cast<const AudioComponent&>(c).soundPath; }),
        ComponentField::Flt("Volume", "Volume", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).volume = SafeParse::ToFloat(d.Get("Volume", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).volume); }),
        ComponentField::Flt("Pitch", "Pitch", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).pitch = SafeParse::ToFloat(d.Get("Pitch", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).pitch); }),
        ComponentField::Flt("Tempo", "Tempo", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).tempo = SafeParse::ToFloat(d.Get("Tempo", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).tempo); }),
        BusField(),
        ComponentField::Bln("AutoPlay", "Auto Play", false,
            [](Component& c, const ComponentDecl& d) {
                auto& ac = static_cast<AudioComponent&>(c);
                bool autoPlay = d.Get("AutoPlay", "false") == "true";
                ac.autoPlay = autoPlay;
                if (autoPlay && !d.Get("Sound").empty() && !ComponentRegistry::IsEditorApply()) ac.Play();
            },
            [](const Component& c) { return ComponentField::BoolStr(static_cast<const AudioComponent&>(c).autoPlay); }),
        ComponentField::Bln("Looping", "Looping", false,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).looping = d.Get("Looping", "false") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const AudioComponent&>(c).looping); }),
    }
});
}
