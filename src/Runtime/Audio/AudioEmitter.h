#pragma once
#include "AudioEffects.h"
#include "Core/AudioAttenuationTypes.h"
#include "raylib.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class Actor;
class AudioSource;

struct AudioListenerState {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 right{ 1.0f, 0.0f, 0.0f };
    Actor* actor = nullptr;
    bool valid = false;
};

using AudioRaycastFn = std::function<bool(const Vector3& from, const Vector3& to, uint32_t mask, bool meshes, Actor* ignore, Actor*& hitActor)>;
using AudioPositionFn = std::function<bool(Vector3& position, Actor*& ignore)>;

namespace AudioSpatialMath {

float CurveGain(int curve, float distance, float minDistance, float maxDistance);
float PanFor(const Vector3& emitter, const Vector3& listener, const Vector3& right, float minDistance);

}

class AudioEmitter {
public:
    AudioEmitter(int emitterId, AudioSource* audioSource, const std::string& attenuationName, const AudioAttenuationSettings& initial);

    int Id() const { return id; }
    AudioSource* Source() const { return source; }
    const std::string& Attenuation() const { return attenuation; }
    AudioAttenuationSettings& Settings() { return settings; }
    void SetAttenuation(const std::string& name, const AudioAttenuationSettings& next);
    void SetSettings(const AudioAttenuationSettings& next) { settings = next; }
    void SetEnabled(bool value) { enabled = value; }
    bool Enabled() const { return enabled; }
    void SetPosition(const Vector3& value, Actor* ignore);
    void SetProvider(AudioPositionFn fn) { provider = std::move(fn); }

    void Update(float deltaTime, const AudioListenerState& listener, const AudioRaycastFn& raycast);
    void Release(bool resetSource);

    float Gain() const { return lastGain; }
    float Pan() const { return lastPan; }
    float Occlusion() const { return occlusion; }

private:
    void UpdateEffect();

    int id;
    AudioSource* source;
    std::string attenuation;
    AudioAttenuationSettings settings;
    bool enabled = true;
    bool hasPosition = false;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Actor* ignoreActor = nullptr;
    AudioPositionFn provider;

    float timer = 0.0f;
    bool occlusionKnown = false;
    float occlusionTarget = 0.0f;
    float occlusion = 0.0f;
    float lastGain = 1.0f;
    float lastPan = 0.0f;
    std::shared_ptr<const AudioEffectChain> occlusionChain;
    std::string occlusionEffectName;
};
