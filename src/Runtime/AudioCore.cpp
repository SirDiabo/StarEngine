#include "AudioCore.h"
#include "Audio/AudioMixer.h"
#include "Core/AudioBusRegistry.h"
#include "Core/AudioEffectRegistry.h"
#include "Core/AudioAttenuationRegistry.h"
#include "Runtime/CollisionTrace.h"
#include "raylib.h"
#include <algorithm>
#include <iterator>
#include <vector>

AudioCore::AudioCore() {
    for (int i = 0; i < kMaxMixerBuses; i++) {
        busVolume[i] = 1.0f;
        busMuted[i] = false;
    }
    AudioEffectRegistry::Get().AddListener([]() { AudioCore::Get().ApplyProjectEffects(); });
    AudioEffectRegistry::Get().AddParamListener([](const std::string& effect, const std::string& param, float value) {
        AudioCore::Get().OnEffectParamChanged(effect, param, value);
    });
    ApplyProjectEffects();
    AudioAttenuationRegistry::Get().AddListener([]() { AudioCore::Get().OnAttenuationChanged(); });
    AudioAttenuationRegistry::Get().AddParamListener([](const std::string&) { AudioCore::Get().OnAttenuationChanged(); });
}

AudioCore& AudioCore::Get() {
    static AudioCore instance;
    return instance;
}

void AudioCore::InitDevice() {
    if (deviceInitialized) return;
    InitAudioDevice();
    AudioMixer::Get().Start();
    deviceInitialized = true;
}

void AudioCore::CloseDevice() {
    if (!deviceInitialized) return;
    instances.clear();
    AudioMixer::Get().StopAllVoices();
    AudioMixer::Get().Shutdown();
    for (int i = 0; i < kMaxMixerBuses; i++) busChains[i].reset();
    retiredChains.clear();
    sampleCache.clear();
    CloseAudioDevice();
    deviceInitialized = false;
}

static bool DefaultAudioRaycast(const Vector3& from, const Vector3& to, uint32_t mask, bool meshes, Actor* ignore, Actor*& hitActor) {
    CollisionTrace::QueryParams params;
    params.ignoreActor = ignore;
    params.mask = mask;
    params.meshes = meshes;
    params.shapes = true;
    CollisionTrace::HitResult hit;
    if (!CollisionTrace::Raycast(from, to, params, hit)) return false;
    hitActor = hit.actor;
    return true;
}

void AudioCore::SetListener(const Vector3& position, const Vector3& right, Actor* actor) {
    listener.position = position;
    listener.right = right;
    listener.actor = actor;
    listener.valid = true;
}

void AudioCore::ClearListener() {
    listener = AudioListenerState();
}

void AudioCore::SetRaycastHook(AudioRaycastFn hook) {
    raycastHook = std::move(hook);
}

int AudioCore::CreateEmitter(AudioSource* source, const std::string& attenuationName) {
    const AudioAttenuationDef* def = AudioAttenuationRegistry::Get().Find(attenuationName);
    if (!source || !def) return 0;
    const int id = nextEmitterId++;
    emitters[id] = std::make_unique<AudioEmitter>(id, source, attenuationName, AudioAttenuationSchema::MakeSettings(*def));
    return id;
}

void AudioCore::DestroyEmitter(int id, bool resetSource) {
    auto it = emitters.find(id);
    if (it == emitters.end()) return;
    it->second->Release(resetSource);
    emitters.erase(it);
}

AudioEmitter* AudioCore::FindEmitter(int id) {
    auto it = emitters.find(id);
    return it == emitters.end() ? nullptr : it->second.get();
}

void AudioCore::UpdateEmitters(float deltaTime) {
    if (emitters.empty()) return;
    const AudioRaycastFn& raycast = raycastHook ? raycastHook : AudioRaycastFn(DefaultAudioRaycast);
    for (auto& kv : emitters) kv.second->Update(deltaTime, listener, raycast);
}

void AudioCore::RefreshEmitter(int id) {
    if (AudioEmitter* emitter = FindEmitter(id)) emitter->Update(0.0f, listener, raycastHook ? raycastHook : AudioRaycastFn(DefaultAudioRaycast));
}

void AudioCore::OnAttenuationChanged() {
    const AudioAttenuationRegistry& registry = AudioAttenuationRegistry::Get();
    for (auto& kv : emitters) {
        const AudioAttenuationDef* def = registry.Find(kv.second->Attenuation());
        if (def) kv.second->SetSettings(AudioAttenuationSchema::MakeSettings(*def));
    }
}

int AudioCore::PlayAssetSpatial(const std::string& assetPath, float volume, float pitch, bool looping, const std::string& busName,
                                const std::string& attenuationName, AudioPositionFn provider) {
    std::unique_ptr<AudioSource> source = AudioSourceRegistry::Get().Create(assetPath);
    if (!source) return 0;

    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
    int bus = ResolveBus(source->DefaultBusName(), busName);
    source->SetBus(bus < 0 ? 0 : bus);

    int emitterId = CreateEmitter(source.get(), attenuationName);
    if (emitterId) {
        AudioEmitter* emitter = FindEmitter(emitterId);
        emitter->SetProvider(std::move(provider));
        emitter->Update(0.0f, listener, raycastHook ? raycastHook : AudioRaycastFn(DefaultAudioRaycast));
    }

    source->Play();
    if (!source->IsPlaying()) {
        if (emitterId) DestroyEmitter(emitterId, false);
        return 0;
    }

    int handle = nextHandle++;
    instances[handle] = std::move(source);
    if (emitterId) instanceEmitters[handle] = emitterId;
    return handle;
}

bool AudioCore::SetInstanceAttenuationEnabled(int handle, bool enabled) {
    auto it = instanceEmitters.find(handle);
    AudioEmitter* emitter = it == instanceEmitters.end() ? nullptr : FindEmitter(it->second);
    if (!emitter) return false;
    emitter->SetEnabled(enabled);
    return true;
}

bool AudioCore::SetInstanceAttenuationParameter(int handle, const std::string& paramId, float value) {
    auto it = instanceEmitters.find(handle);
    AudioEmitter* emitter = it == instanceEmitters.end() ? nullptr : FindEmitter(it->second);
    return emitter && emitter->Settings().SetValue(paramId, value);
}

bool AudioCore::SetInstanceAttenuation(int handle, const std::string& attenuationName) {
    AudioSource* source = FindInstance(handle);
    const AudioAttenuationDef* def = AudioAttenuationRegistry::Get().Find(attenuationName);
    if (!source || !def) return false;
    auto it = instanceEmitters.find(handle);
    AudioEmitter* emitter = it == instanceEmitters.end() ? nullptr : FindEmitter(it->second);
    if (!emitter) {
        const int id = CreateEmitter(source, attenuationName);
        if (!id) return false;
        instanceEmitters[handle] = id;
        return true;
    }
    emitter->SetAttenuation(attenuationName, AudioAttenuationSchema::MakeSettings(*def));
    return true;
}

bool AudioCore::SetInstancePosition(int handle, const Vector3& position) {
    auto it = instanceEmitters.find(handle);
    AudioEmitter* emitter = it == instanceEmitters.end() ? nullptr : FindEmitter(it->second);
    if (!emitter) return false;
    emitter->SetProvider(nullptr);
    emitter->SetPosition(position, nullptr);
    return true;
}

void AudioCore::Tick(float deltaTime) {
    PruneRetired();
    UpdateEmitters(deltaTime);
    std::vector<int> finished;
    for (auto& kv : instances) {
        kv.second->Tick(deltaTime);
        if (!kv.second->IsPlaying() && !kv.second->IsPaused()) finished.push_back(kv.first);
    }
    for (int handle : finished) {
        auto emitter = instanceEmitters.find(handle);
        if (emitter != instanceEmitters.end()) {
            DestroyEmitter(emitter->second, false);
            instanceEmitters.erase(emitter);
        }
        instances.erase(handle);
        auto fx = instanceEffects.find(handle);
        if (fx != instanceEffects.end()) {
            RetireEffectChain(fx->second.chain);
            instanceEffects.erase(fx);
        }
    }
}

std::shared_ptr<const SampleData> AudioCore::GetSampleData(const std::string& resolvedWavPath) {
    auto it = sampleCache.find(resolvedWavPath);
    if (it != sampleCache.end()) return it->second;

    std::string error;
    std::shared_ptr<SampleData> loaded = WavFile::LoadAll(resolvedWavPath, error);
    if (!loaded) {
        TraceLog(LOG_ERROR, "Audio: could not load '%s': %s", resolvedWavPath.c_str(), error.c_str());
        return nullptr;
    }
    sampleCache[resolvedWavPath] = loaded;
    return loaded;
}

void AudioCore::ClearSampleCache() {
    sampleCache.clear();
}

int AudioCore::PlayAsset(const std::string& assetPath, float volume, float pitch, bool looping, const std::string& busName) {
    std::unique_ptr<AudioSource> source = AudioSourceRegistry::Get().Create(assetPath);
    if (!source) return 0;

    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLooping(looping);
    int bus = ResolveBus(source->DefaultBusName(), busName);
    source->SetBus(bus < 0 ? 0 : bus);
    source->Play();
    if (!source->IsPlaying()) return 0;

    int handle = nextHandle++;
    instances[handle] = std::move(source);
    return handle;
}

AudioSource* AudioCore::FindInstance(int handle) {
    auto it = instances.find(handle);
    return it == instances.end() ? nullptr : it->second.get();
}

void AudioCore::StopInstance(int handle) {
    if (AudioSource* source = FindInstance(handle)) source->Stop();
}

void AudioCore::StopAll() {
    for (auto& kv : instances) kv.second->Stop();
    for (auto& kv : instanceEmitters) DestroyEmitter(kv.second, false);
    instanceEmitters.clear();
    instances.clear();
    AudioMixer::Get().StopAllVoices();
}

void AudioCore::ApplyBus(int index) {
    if (index < 0 || index >= kMaxMixerBuses) return;
    AudioMixer::Get().SetBusGain(index, busMuted[index] ? 0.0f : busVolume[index]);
}

bool AudioCore::SetBusVolume(const std::string& busName, float volume) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    busVolume[index] = volume < 0.0f ? 0.0f : volume;
    ApplyBus(index);
    return true;
}

bool AudioCore::GetBusVolume(const std::string& busName, float& volume) const {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    volume = busVolume[index];
    return true;
}

bool AudioCore::SetBusMuted(const std::string& busName, bool muted) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    busMuted[index] = muted;
    ApplyBus(index);
    return true;
}

bool AudioCore::IsBusMuted(const std::string& busName, bool& muted) const {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    muted = busMuted[index];
    return true;
}

void AudioCore::ResetBuses() {
    for (int i = 0; i < kMaxMixerBuses; i++) {
        busVolume[i] = 1.0f;
        busMuted[i] = false;
        ApplyBus(i);
    }
    ApplyProjectEffects();
}

int AudioCore::ResolveBus(const std::string& defaultBusName, const std::string& overrideName) const {
    const AudioBusRegistry& registry = AudioBusRegistry::Get();
    if (!overrideName.empty() && overrideName != AudioBusRegistry::AssetDefaultLabel()) {
        int index = registry.IndexOf(overrideName);
        if (index >= 0) return index;
        TraceLog(LOG_WARNING, "Audio: unknown bus '%s', using the asset default", overrideName.c_str());
    }
    int index = registry.IndexOf(defaultBusName);
    return index < 0 ? 0 : index;
}

void AudioCore::PruneRetired() {
    if (retiredChains.empty()) return;
    auto now = std::chrono::steady_clock::now();
    retiredChains.erase(std::remove_if(retiredChains.begin(), retiredChains.end(),
        [&](const RetiredChain& r) { return now - r.since > std::chrono::seconds(3); }), retiredChains.end());
}

bool AudioCore::SetBusEffectsByIndex(int index, const std::vector<std::string>& effectNames) {
    if (index < 0 || index >= kMaxMixerBuses) return false;

    std::shared_ptr<const AudioEffectChain> chain = BuildEffectChain(effectNames, busChains[index]);
    if (busChains[index]) retiredChains.push_back({ busChains[index], std::chrono::steady_clock::now() });
    busChains[index] = chain;
    AudioMixer::Get().SetBusChain(index, chain);
    PruneRetired();
    return true;
}

std::shared_ptr<const AudioEffectChain> AudioCore::BuildEffectChain(const std::vector<std::string>& effectNames, const std::shared_ptr<const AudioEffectChain>& reuseFrom) {
    if (effectNames.empty()) return nullptr;

    auto chain = std::make_shared<AudioEffectChain>();
    const AudioEffectRegistry& registry = AudioEffectRegistry::Get();
    std::vector<bool> taken(reuseFrom ? reuseFrom->effects.size() : 0, false);

    for (const std::string& name : effectNames) {
        const AudioEffectDef* def = registry.Find(name);
        if (!def) {
            TraceLog(LOG_WARNING, "Audio: unknown effect '%s'", name.c_str());
            continue;
        }

        std::shared_ptr<AudioEffect> effect;
        if (reuseFrom) {
            for (size_t i = 0; i < reuseFrom->effects.size(); i++) {
                if (!taken[i] && reuseFrom->names[i] == name && reuseFrom->effects[i]->Type() == def->type) {
                    effect = reuseFrom->effects[i];
                    taken[i] = true;
                    break;
                }
            }
        }
        if (!effect) effect = CreateAudioEffect(def->type, kMixerSampleRate);
        if (!effect) {
            TraceLog(LOG_WARNING, "Audio: effect '%s' has unknown type '%s'", name.c_str(), def->type.c_str());
            continue;
        }

        const AudioEffectTypeInfo* info = effect->Info();
        for (size_t i = 0; i < info->params.size(); i++) {
            effect->SetParam((int)i, AudioEffectRegistry::ParamValue(*def, info->params[i].id));
        }
        chain->effects.push_back(std::move(effect));
        chain->names.push_back(name);
    }
    if (chain->effects.empty()) return nullptr;

    trackedChains.erase(std::remove_if(trackedChains.begin(), trackedChains.end(),
        [](const std::weak_ptr<const AudioEffectChain>& w) { return w.expired(); }), trackedChains.end());
    trackedChains.push_back(chain);
    return chain;
}

void AudioCore::RetireEffectChain(const std::shared_ptr<const AudioEffectChain>& chain) {
    if (!chain) return;
    retiredChains.push_back({ chain, std::chrono::steady_clock::now() });
    PruneRetired();
}

bool AudioCore::SetChainEffectParameter(const AudioEffectChain& chain, const std::string& effectName, const std::string& paramId, float value) {
    bool applied = false;
    for (size_t i = 0; i < chain.effects.size(); i++) {
        if (chain.names[i] != effectName) continue;
        int param = AudioEffectTypes::ParamIndex(*chain.effects[i]->Info(), paramId);
        if (param < 0) continue;
        chain.effects[i]->SetParam(param, value);
        applied = true;
    }
    return applied;
}

std::vector<std::string> AudioCore::GetBusEffects(const std::string& busName) const {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0 || !busChains[index]) return {};
    return busChains[index]->names;
}

bool AudioCore::SetBusEffects(const std::string& busName, const std::vector<std::string>& effectNames) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0) return false;
    return SetBusEffectsByIndex(index, effectNames);
}

bool AudioCore::AddBusEffect(const std::string& busName, const std::string& effectName) {
    std::vector<std::string> names = GetBusEffects(busName);
    names.push_back(effectName);
    return SetBusEffects(busName, names);
}

bool AudioCore::RemoveBusEffect(const std::string& busName, const std::string& effectName) {
    std::vector<std::string> names = GetBusEffects(busName);
    auto found = std::find(names.begin(), names.end(), effectName);
    if (found == names.end()) return false;
    names.erase(found);
    return SetBusEffects(busName, names);
}

bool AudioCore::PopBusEffect(const std::string& busName) {
    std::vector<std::string> names = GetBusEffects(busName);
    if (names.empty()) return false;
    names.pop_back();
    return SetBusEffects(busName, names);
}

bool AudioCore::ClearBusEffects(const std::string& busName) {
    return SetBusEffects(busName, {});
}

bool AudioCore::SetBusEffectParameter(const std::string& busName, const std::string& effectName, const std::string& paramId, float value) {
    int index = AudioBusRegistry::Get().IndexOf(busName);
    if (index < 0 || !busChains[index]) return false;
    return SetChainEffectParameter(*busChains[index], effectName, paramId, value);
}

bool AudioCore::ApplyInstanceEffects(int handle, InstanceEffects& entry) {
    AudioSource* source = FindInstance(handle);
    if (!source) return false;
    std::shared_ptr<const AudioEffectChain> next = BuildEffectChain(entry.names, entry.chain);
    RetireEffectChain(entry.chain);
    entry.chain = next;
    source->SetEffectChain(entry.chain);
    return true;
}

bool AudioCore::PushInstanceEffect(int handle, const std::string& effectName) {
    if (!FindInstance(handle) || !AudioEffectRegistry::Get().Find(effectName)) return false;
    InstanceEffects& entry = instanceEffects[handle];
    entry.names.push_back(effectName);
    return ApplyInstanceEffects(handle, entry);
}

bool AudioCore::PopInstanceEffect(int handle, const std::string& effectName) {
    auto it = instanceEffects.find(handle);
    if (it == instanceEffects.end() || it->second.names.empty()) return false;
    std::vector<std::string>& names = it->second.names;
    if (effectName.empty()) {
        names.pop_back();
    } else {
        auto found = std::find(names.rbegin(), names.rend(), effectName);
        if (found == names.rend()) return false;
        names.erase(std::next(found).base());
    }
    return ApplyInstanceEffects(handle, it->second);
}

bool AudioCore::ClearInstanceEffects(int handle) {
    auto it = instanceEffects.find(handle);
    if (it == instanceEffects.end()) return FindInstance(handle) != nullptr;
    it->second.names.clear();
    return ApplyInstanceEffects(handle, it->second);
}

bool AudioCore::SetInstanceEffectParameter(int handle, const std::string& effectName, const std::string& paramId, float value) {
    auto it = instanceEffects.find(handle);
    return it != instanceEffects.end() && it->second.chain && SetChainEffectParameter(*it->second.chain, effectName, paramId, value);
}

void AudioCore::OnEffectParamChanged(const std::string& effectName, const std::string& paramId, float value) {
    for (const auto& weak : trackedChains) {
        if (std::shared_ptr<const AudioEffectChain> chain = weak.lock()) SetChainEffectParameter(*chain, effectName, paramId, value);
    }
}

void AudioCore::ApplyProjectEffects() {
    const AudioEffectRegistry& effects = AudioEffectRegistry::Get();
    const AudioBusRegistry& buses = AudioBusRegistry::Get();
    for (int i = 0; i < kMaxMixerBuses; i++) {
        std::string name = buses.NameAt(i);
        std::vector<std::string> names = name.empty() ? std::vector<std::string>() : effects.BusEffects(name);
        if (names.empty() && !busChains[i]) continue;
        SetBusEffectsByIndex(i, names);
    }
}
