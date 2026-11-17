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

    bool operator==(const ActorSettings& o) const {
        return movementSpeed == o.movementSpeed && acceleration == o.acceleration
            && brakingDeceleration == o.brakingDeceleration && mass == o.mass
            && showMouseCursor == o.showMouseCursor && useControlYaw == o.useControlYaw
            && useControlPitch == o.useControlPitch && useControlRoll == o.useControlRoll;
    }
    bool operator!=(const ActorSettings& o) const { return !(*this == o); }
};
