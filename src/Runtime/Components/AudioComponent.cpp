#include "AudioComponent.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/SafeParse.h"
#include "Runtime/AudioCore.h"
#include "Core/AudioBusRegistry.h"
#include "Core/AudioEffectRegistry.h"
#include "Core/AudioAttenuationRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/AudioCore.h"
#include <algorithm>
#include <cstdlib>
#include <iterator>

AudioComponent::AudioComponent() = default;
AudioComponent::~AudioComponent() {
    DestroyEmitter(false);
    AudioCore::Get().RetireEffectChain(effectChain);
}

void AudioComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Sound Path", &soundPath);
    outProperties.AddBool("Auto Play", &autoPlay);
    outProperties.AddBool("Looping", &looping);
    outProperties.AddFloat("Volume", &volume);
    outProperties.AddFloat("Pitch", &pitch);
    outProperties.AddFloat("Tempo", &tempo);
    outProperties.AddFloat("Pan", &pan);
    outProperties.AddString("Bus", &bus);
}

void AudioComponent::DestroyEmitter(bool resetSource) {
    if (!emitterId) return;
    AudioCore::Get().DestroyEmitter(emitterId, resetSource);
    emitterId = 0;
}

AudioSource* AudioComponent::EnsureSource() {
    if (soundPath.empty()) {
        DestroyEmitter(false);
        appliedAttenuationKey.clear();
        source.reset();
        sourcePath.clear();
        return nullptr;
    }
    if (!source || sourcePath != soundPath) {
        DestroyEmitter(false);
        appliedAttenuationKey.clear();
        source = AudioSourceRegistry::Get().Create(soundPath);
        sourcePath = soundPath;
        appliedBusKey.clear();
        appliedEffectsKey.clear();
    }
    return source.get();
}

void AudioComponent::SyncSource() {
    if (!source) return;
    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
    source->SetPan(pan);
    if (source->SupportsTempo()) source->SetTempo(tempo);

    std::string key = sourcePath + "|" + bus;
    if (key != appliedBusKey) {
        appliedBusKey = key;
        int index = AudioCore::Get().ResolveBus(source->DefaultBusName(), bus);
        source->SetBus(index < 0 ? 0 : index);
    }

    std::string effectsKey = sourcePath + "|" + ComponentField::JoinList(effects);
    if (effectsKey != appliedEffectsKey) {
        appliedEffectsKey = effectsKey;
        std::shared_ptr<const AudioEffectChain> next = AudioCore::Get().BuildEffectChain(effects, effectChain);
        AudioCore::Get().RetireEffectChain(effectChain);
        effectChain = next;
        source->SetEffectChain(effectChain);
    }

    std::string attenuationKey = sourcePath + "|" + attenuation;
    if (attenuationKey != appliedAttenuationKey) {
        appliedAttenuationKey = attenuationKey;
        DestroyEmitter(true);
        if (!attenuation.empty()) emitterId = AudioCore::Get().CreateEmitter(source.get(), attenuation);
        if (AudioEmitter* emitter = AudioCore::Get().FindEmitter(emitterId)) {
            emitter->SetEnabled(attenuationEnabled);
            if (owningActor) emitter->SetPosition(owningActor->GetComponentWorldPosition(this), owningActor);
            AudioCore::Get().RefreshEmitter(emitterId);
        }
    }
    if (AudioEmitter* emitter = AudioCore::Get().FindEmitter(emitterId)) {
        if (owningActor) emitter->SetPosition(owningActor->GetComponentWorldPosition(this), owningActor);
    }
}

bool AudioComponent::SetAttenuation(const std::string& name) {
    const std::string next = name == "None" ? std::string() : name;
    if (!next.empty() && !AudioAttenuationRegistry::Get().Find(next)) return false;
    attenuation = next;
    if (source) SyncSource();
    return true;
}

bool AudioComponent::SetAttenuationEnabled(bool enabled) {
    attenuationEnabled = enabled;
    AudioEmitter* emitter = AudioCore::Get().FindEmitter(emitterId);
    if (emitter) emitter->SetEnabled(enabled);
    return emitter != nullptr;
}

bool AudioComponent::SetAttenuationParameter(const std::string& paramId, float value) {
    AudioEmitter* emitter = AudioCore::Get().FindEmitter(emitterId);
    return emitter && emitter->Settings().SetValue(paramId, value);
}

void AudioComponent::PushEffect(const std::string& name) {
    effects.push_back(name);
    if (source) SyncSource();
}

bool AudioComponent::PopEffect(const std::string& name) {
    if (effects.empty()) return false;
    if (name.empty()) {
        effects.pop_back();
    } else {
        auto found = std::find(effects.rbegin(), effects.rend(), name);
        if (found == effects.rend()) return false;
        effects.erase(std::next(found).base());
    }
    if (source) SyncSource();
    return true;
}

void AudioComponent::ClearEffects() {
    effects.clear();
    if (source) SyncSource();
}

bool AudioComponent::SetEffectParameter(const std::string& effectName, const std::string& paramId, float value) {
    return effectChain && AudioCore::SetChainEffectParameter(*effectChain, effectName, paramId, value);
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

bool AudioComponent::SetParameter(const std::string& path, const AudioParamValue& value) {
    if (!EnsureSource()) return false;
    return source->SetParameter(path, value);
}

bool AudioComponent::GetParameter(const std::string& path, AudioParamValue& out) const {
    return source && source->GetParameter(path, out);
}

namespace {

ComponentFieldDesc AttenuationField() {
    ComponentFieldDesc d = ComponentField::Enm("Attenuation", "Attenuation", {}, "None",
        [](Component& c, const ComponentDecl& decl) {
            std::string value = decl.Get("Attenuation", "None");
            static_cast<AudioComponent&>(c).attenuation = value == "None" ? std::string() : value;
        },
        [](const Component& c) -> std::string {
            const std::string& value = static_cast<const AudioComponent&>(c).attenuation;
            return value.empty() ? std::string("None") : value;
        });
    d.enumProvider = []() {
        std::vector<std::string> options = { "None" };
        for (const std::string& name : AudioAttenuationRegistry::Get().Names()) options.push_back(name);
        return options;
    };
    return d;
}

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
        ComponentField::Flt("Pan", "Pan", 0.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).pan = SafeParse::ToFloat(d.Get("Pan", "0")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).pan); }),
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
        AttenuationField(),
        ComponentField::Lst("Effects", "Effects", []() { return AudioEffectRegistry::Get().Names(); },
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).effects = ComponentField::SplitList(d.Get("Effects", "")); },
            [](const Component& c) { return ComponentField::JoinList(static_cast<const AudioComponent&>(c).effects); }),
    }
});
}
