#include "AudioEmitter.h"
#include "AudioSource.h"
#include "Core/AudioEffectRegistry.h"
#include "Runtime/AudioCore.h"
#include <algorithm>
#include <cmath>

namespace AudioSpatialMath {

float CurveGain(int curve, float distance, float minDistance, float maxDistance) {
    if (distance <= minDistance) return 1.0f;
    if (distance >= maxDistance) return 0.0f;
    const float span = std::max(0.0001f, maxDistance - minDistance);
    const float t = std::clamp((distance - minDistance) / span, 0.0f, 1.0f);
    switch (curve) {
        case 1: {
            const float inner = std::max(0.0001f, minDistance);
            const float farValue = inner / maxDistance;
            const float value = inner / distance;
            return std::clamp((value - farValue) / std::max(0.0001f, 1.0f - farValue), 0.0f, 1.0f);
        }
        case 2: return std::clamp(1.0f - std::log10(1.0f + 9.0f * t), 0.0f, 1.0f);
        case 3: return 1.0f - t * t * (3.0f - 2.0f * t);
        default: return 1.0f - t;
    }
}

float PanFor(const Vector3& emitter, const Vector3& listener, const Vector3& right, float minDistance) {
    const Vector3 delta = { emitter.x - listener.x, emitter.y - listener.y, emitter.z - listener.z };
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (distance < 0.0001f) return 0.0f;
    const float side = (delta.x * right.x + delta.y * right.y + delta.z * right.z) / distance;
    const float closeness = std::clamp(distance / std::max(0.001f, minDistance), 0.0f, 1.0f);
    return std::clamp(side, -1.0f, 1.0f) * closeness;
}

}

AudioEmitter::AudioEmitter(int emitterId, AudioSource* audioSource, const std::string& attenuationName, const AudioAttenuationSettings& initial)
    : id(emitterId), source(audioSource), attenuation(attenuationName), settings(initial) {}

void AudioEmitter::SetAttenuation(const std::string& name, const AudioAttenuationSettings& next) {
    attenuation = name;
    settings = next;
    occlusionKnown = false;
}

void AudioEmitter::SetPosition(const Vector3& value, Actor* ignore) {
    position = value;
    ignoreActor = ignore;
    hasPosition = true;
}

void AudioEmitter::Update(float deltaTime, const AudioListenerState& listener, const AudioRaycastFn& raycast) {
    float gain = 1.0f;
    float pan = 0.0f;
    float target = 0.0f;

    bool active = enabled && settings.RequiresPosition() && listener.valid;
    if (active && provider) hasPosition = provider(position, ignoreActor);
    active = active && hasPosition;

    if (active) {
        const Vector3 delta = { position.x - listener.position.x, position.y - listener.position.y, position.z - listener.position.z };
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (settings.distanceEnabled) gain = AudioSpatialMath::CurveGain(settings.curve, distance, settings.minDistance, settings.maxDistance);
        if (settings.spatialization) pan = AudioSpatialMath::PanFor(position, listener.position, listener.right, settings.minDistance) * settings.panStrength * settings.spatialBlend;

        if (settings.occlusion) {
            if (gain > 0.0001f) {
                timer += deltaTime;
                if (!occlusionKnown || timer >= settings.occlusionInterval) {
                    const bool first = !occlusionKnown;
                    timer = 0.0f;
                    occlusionKnown = true;
                    Actor* hit = nullptr;
                    const bool blocked = raycast && raycast(position, listener.position, settings.occlusionMask, settings.occlusionMeshes, ignoreActor, hit) && (listener.actor == nullptr || hit != listener.actor);
                    occlusionTarget = blocked ? 1.0f : 0.0f;
                    if (first) occlusion = occlusionTarget;
                }
            }
            target = occlusionTarget;
        }
    }

    if (deltaTime > 0.0f) {
        const float rate = 1.0f - std::exp(-deltaTime / std::max(0.001f, settings.occlusionSmoothing));
        occlusion += (target - occlusion) * rate;
    }
    if (target == 0.0f && occlusion < 0.0005f) occlusion = 0.0f;
    if (target == 1.0f && occlusion > 0.9995f) occlusion = 1.0f;

    gain *= 1.0f - (1.0f - settings.occludedVolume) * occlusion;
    lastGain = gain;
    lastPan = pan;
    UpdateEffect();
    if (source) source->SetSpatial(gain, pan);
}

void AudioEmitter::UpdateEffect() {
    AudioEffectRegistry& registry = AudioEffectRegistry::Get();
    const bool wanted = settings.occlusion && !settings.occludedEffect.empty() && occlusion > 0.002f && registry.Find(settings.occludedEffect) != nullptr;
    if (!wanted) {
        if (occlusionChain) {
            if (source) source->SetSpatialChain(nullptr);
            AudioCore::Get().RetireEffectChain(occlusionChain);
            occlusionChain.reset();
            occlusionEffectName.clear();
        }
        return;
    }
    if (!occlusionChain || occlusionEffectName != settings.occludedEffect) {
        AudioCore::Get().RetireEffectChain(occlusionChain);
        occlusionChain = AudioCore::Get().BuildEffectChain({ settings.occludedEffect });
        occlusionEffectName = settings.occludedEffect;
        if (source) source->SetSpatialChain(occlusionChain);
    }
    if (occlusionChain && !occlusionChain->effects.empty()) {
        occlusionChain->effects[0]->SetParam(0, occlusion * registry.GetParam(occlusionEffectName, "mix"));
    }
}

void AudioEmitter::Release(bool resetSource) {
    if (resetSource && source) {
        source->SetSpatial(1.0f, 0.0f);
        source->SetSpatialChain(nullptr);
    }
    if (occlusionChain) AudioCore::Get().RetireEffectChain(occlusionChain);
    occlusionChain.reset();
}
