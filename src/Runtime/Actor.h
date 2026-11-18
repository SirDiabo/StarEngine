#pragma once
#include "Object.h"
#include "ActorBehavior.h"
#include "ActorSettings.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PhysicsComponent.h"
#include "raylib.h"
#include <vector>
#include <cstdint>

class Component;

struct TickViewers {
    Vector3 points[2]{};
    int count = 0;
};

class Actor : public Object {
public:
    Actor();
    ~Actor() override;

    const char* GetClassName() const override { return "Actor"; }
    std::string GetObjectKind() const override { return actorClass.empty() ? "Actor" : actorClass; }
    void GetProperties(PropertyList& outProperties) override;

    const std::string& GetActorClass() const { return actorClass; }
    void SetActorClass(const std::string& newClass);

    void BeginPlay();
    void Tick(float deltaTime);
    void EndPlay();

    void SetBehavior(ActorBehavior* behavior);
    ActorBehavior* GetBehavior() const { return behavior; }
    void SaveState(const std::string& saveFile = "");
    void LoadState(const std::string& saveFile = "");
    Matrix GetWorldMatrix() const;

    uint32_t GetNetId() const { return netId; }
    void SetNetId(uint32_t id);
    static Actor* FindByNetId(uint32_t id);

    static Vector3 ComputeWorldPosition(Vector3 localPos, Vector3 parentPos, Vector3 parentRot, Vector3 parentScale);
    static Vector3 ComputeWorldRotation(Vector3 localRot, Vector3 parentRot);
    static Vector3 ComputeWorldScale(Vector3 localScale, Vector3 parentScale);

    Vector3 GetComponentWorldScale(Component* comp) const;
    void SetComponentWorldPosition(Component* comp, Vector3 worldPos);
    void SetComponentWorldRotation(Component* comp, Vector3 worldRot);
    void SetComponentWorldScale(Component* comp, Vector3 worldScale);

    void GetComponentWorldTransform(Component* comp, Vector3& outPos, Vector3& outRot, Vector3& outScale) const;
    Vector3 GetComponentWorldPosition(Component* comp) const;
    Vector3 GetComponentWorldRotation(Component* comp) const;

    Vector3 position{ 0, 0, 0 };
    Vector3 rotation{ 0, 0, 0 };
    Vector3 scale{ 1, 1, 1 };

    bool showMouseCursor = ActorDefaults::ShowMouseCursor;

    bool hasBegunPlay = false;

    Vector3 controlRotation{ 0, 0, 0 };
    Vector3 pendingMovementInput{ 0, 0, 0 };

    bool useControlYaw = ActorDefaults::UseControlYaw;
    bool useControlPitch = ActorDefaults::UseControlPitch;
    bool useControlRoll = ActorDefaults::UseControlRoll;

    float MovementSpeed = ActorDefaults::MovementSpeed;
    float acceleration = ActorDefaults::Acceleration;
    float brakingDeceleration = ActorDefaults::BrakingDeceleration;
    float mass = ActorDefaults::Mass;
    Vector3 velocity{ 0, 0, 0 };

    float tickRate = ActorDefaults::TickRate;
    float maxTickDistance = ActorDefaults::MaxTickDistance;
    float farTickRate = ActorDefaults::FarTickRate;
    float maxDrawDistance = ActorDefaults::MaxDrawDistance;
    bool PrepareTick(const TickViewers& viewers, float deltaTime, float& outStep);

    void SetControlRotation(Vector3 value);
    void AddControlYawInput(float value);
    void AddControlPitchInput(float value);
    void AddControlRollInput(float value);
    void AddMovementInput(Vector3 worldDirection, float scaleValue);
    void ApplySettings(const ActorSettings& settings);
    void SetVelocity(Vector3 value);
    void AddImpulse(Vector3 impulse);
    void MarkVelocityIntegrated() { velocityIntegrated = true; }
    bool IsMovementDriven() const { return movementDriven; }

    void AddComponent(Component* component, Component* parent = nullptr);
    void RemoveComponent(Component* component);
    Component* GetRootComponent();
    const std::vector<Component*>& GetComponents() const { return components; }
    Component* FindComponentByName(const std::string& name) const;
    MeshComponent* GetMeshComponent() const { return FindComponentOfType<MeshComponent>(); }
    CollisionComponent* GetCollisionComponent() const { return FindComponentOfType<CollisionComponent>(); }
    AudioComponent* GetAudioComponent() const { return FindComponentOfType<AudioComponent>(); }
    CameraComponent* GetCameraComponent() const { return FindComponentOfType<CameraComponent>(); }
    InputComponent* GetInputComponent() const { return FindComponentOfType<InputComponent>(); }
    PhysicsComponent* GetPhysicsComponent() const { return FindComponentOfType<PhysicsComponent>(); }

protected:
    virtual void OnBeginPlay() {}
    virtual void OnTick(float deltaTime) {}
    virtual void OnEndPlay() {}

private:
    void ApplyMovementInput(float dt);

    template<typename T>
    T* FindComponentOfType() const {
        for (Component* c : components) {
            if (T* match = dynamic_cast<T*>(c)) return match;
        }
        return nullptr;
    }

    ActorBehavior* behavior = nullptr;
    std::string actorClass;
    std::vector<Component*> components;
    Component* rootComponent = nullptr;
    uint32_t netId = 0;
    bool velocityIntegrated = false;
    bool receivedMovementInput = false;
    bool movementDriven = false;
    float tickAccumulator = 0.0f;
};
