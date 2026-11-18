#pragma once

namespace ActorDefaults {

constexpr float MovementSpeed = 600.0f;
constexpr float Acceleration = 4000.0f;
constexpr float BrakingDeceleration = 6000.0f;
constexpr float Mass = 1.0f;
constexpr bool ShowMouseCursor = true;
constexpr bool UseControlYaw = true;
constexpr bool UseControlPitch = false;
constexpr bool UseControlRoll = false;
constexpr float TickRate = 0.0f;
constexpr float MaxTickDistance = 0.0f;
constexpr float FarTickRate = 0.0f;
constexpr float MaxDrawDistance = 0.0f;

}

struct ActorSettings {
    float movementSpeed = ActorDefaults::MovementSpeed;
    float acceleration = ActorDefaults::Acceleration;
    float brakingDeceleration = ActorDefaults::BrakingDeceleration;
    float mass = ActorDefaults::Mass;
    bool showMouseCursor = ActorDefaults::ShowMouseCursor;
    bool useControlYaw = ActorDefaults::UseControlYaw;
    bool useControlPitch = ActorDefaults::UseControlPitch;
    bool useControlRoll = ActorDefaults::UseControlRoll;
    float tickRate = ActorDefaults::TickRate;
    float maxTickDistance = ActorDefaults::MaxTickDistance;
    float farTickRate = ActorDefaults::FarTickRate;
    float maxDrawDistance = ActorDefaults::MaxDrawDistance;

    bool operator==(const ActorSettings& o) const {
        return movementSpeed == o.movementSpeed && acceleration == o.acceleration
            && brakingDeceleration == o.brakingDeceleration && mass == o.mass
            && showMouseCursor == o.showMouseCursor && useControlYaw == o.useControlYaw
            && useControlPitch == o.useControlPitch && useControlRoll == o.useControlRoll
            && tickRate == o.tickRate && maxTickDistance == o.maxTickDistance
            && farTickRate == o.farTickRate && maxDrawDistance == o.maxDrawDistance;
    }
    bool operator!=(const ActorSettings& o) const { return !(*this == o); }
};
