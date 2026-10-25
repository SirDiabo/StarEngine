#pragma once
#include "Object.h"
#include "ActorBehavior.h"
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

    bool showMouseCursor = true;

    bool hasBegunPlay = false;

    Vector3 controlRotation{ 0, 0, 0 };
    Vector3 pendingMovementInput{ 0, 0, 0 };

    bool useControlYaw = true;
    bool useControlPitch = false;
    bool useControlRoll = false;

    float MovementSpeed = 600.0f;

    void AddControlYawInput(float value);
    void AddControlPitchInput(float value);
    void AddControlRollInput(float value);
    void AddMovementInput(Vector3 worldDirection, float scaleValue);

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
};
