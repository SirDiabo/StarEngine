#include "AudioComponent.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/SafeParse.h"
#include "Runtime/AudioCore.h"
#include "raylib.h"
#include <cstdlib>

void AudioComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Sound Path", &soundPath);
    outProperties.AddBool("Auto Play", &autoPlay);
    outProperties.AddBool("Looping", &looping);
    outProperties.AddFloat("Volume", &volume);
    outProperties.AddFloat("Pitch", &pitch);
}

void AudioComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);

    if (!wantsPlaying || paused || soundPath.empty()) return;

    Sound* snd = AudioCore::Get().GetOrLoadCookedSound(soundPath);
    if (!snd) return;

    SetSoundVolume(*snd, volume);
    SetSoundPitch(*snd, pitch);
    if (looping && !IsSoundPlaying(*snd)) {
        PlaySound(*snd);
    }
}

void AudioComponent::Play() {
    wantsPlaying = true;
    paused = false;
    Sound* snd = AudioCore::Get().GetOrLoadCookedSound(soundPath);
    if (!snd) return;
    SetSoundVolume(*snd, volume);
    SetSoundPitch(*snd, pitch);
    PlaySound(*snd);
}

void AudioComponent::Stop() {
    wantsPlaying = false;
    paused = false;
    Sound* snd = AudioCore::Get().GetOrLoadCookedSound(soundPath);
    if (!snd) return;
    StopSound(*snd);
}

void AudioComponent::Pause() {
    if (!wantsPlaying || paused) return;
    paused = true;
    Sound* snd = AudioCore::Get().GetOrLoadCookedSound(soundPath);
    if (!snd) return;
    PauseSound(*snd);
}

void AudioComponent::Resume() {
    if (!wantsPlaying || !paused) return;
    paused = false;
    Sound* snd = AudioCore::Get().GetOrLoadCookedSound(soundPath);
    if (!snd) return;
    ResumeSound(*snd);
}

namespace {
ComponentRegistrar registrar_AudioComponent({
    "AudioComponent",
    []() -> Component* { return new AudioComponent(); },
    {
        ComponentField::Asset("Sound", "Sound Path", ".starsound",
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).soundPath = d.Get("Sound"); }, [](const Component& c) { return static_cast<const AudioComponent&>(c).soundPath; }),
        ComponentField::Flt("Volume", "Volume", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).volume = SafeParse::ToFloat(d.Get("Volume", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).volume); }),
        ComponentField::Flt("Pitch", "Pitch", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<AudioComponent&>(c).pitch = SafeParse::ToFloat(d.Get("Pitch", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const AudioComponent&>(c).pitch); }),
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
