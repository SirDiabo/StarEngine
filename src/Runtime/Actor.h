#pragma once
#include "Object.h"
#include "GraphBehavior.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CameraComponent.h"
#include "Components/InputComponent.h"
#include "raylib.h"
#include <vector>
#include <cstdint>

class Component;

class Actor : public Object {
public:
    Actor();
    ~Actor() override;

    const char* GetClassName() const override { return "Actor"; }
    void GetProperties(PropertyList& outProperties) override;

    void BeginPlay();
    void Tick(float deltaTime);
    void EndPlay();

    void SetGraphBehavior(GraphBehavior* behavior);
    GraphBehavior* GetGraphBehavior() const { return graphBehavior; }
    Matrix GetWorldMatrix() const;

    uint32_t GetNetId() const { return netId; }
    void SetNetId(uint32_t id);
    static Actor* FindByNetId(uint32_t id);

    static Vector3 ComputeWorldPosition(Vector3 localPos, Vector3 parentPos, Vector3 parentRot, Vector3 parentScale);
    static Vector3 ComputeWorldRotation(Vector3 localRot, Vector3 parentRot);
    static Vector3 ComputeWorldScale(Vector3 localScale, Vector3 parentScale);

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

    void AddComponent(Component* component);
    void RemoveComponent(Component* component);
    Component* GetRootComponent();
    const std::vector<Component*>& GetComponents() const { return components; }
    Component* FindComponentByName(const std::string& name) const;
    MeshComponent* GetMeshComponent() const { return meshComponent; }
    CollisionComponent* GetCollisionComponent() const { return collisionComponent; }
    AudioComponent* GetAudioComponent() const { return audioComponent; }
    CameraComponent* GetCameraComponent() const { return cameraComponent; }
    InputComponent* GetInputComponent() const { return inputComponent; }

protected:
    virtual void OnBeginPlay() {}
    virtual void OnTick(float deltaTime) {}
    virtual void OnEndPlay() {}

private:
    GraphBehavior* graphBehavior = nullptr;
    std::vector<Component*> components;
    Component* rootComponent = nullptr;
    MeshComponent* meshComponent = nullptr;
    CollisionComponent* collisionComponent = nullptr;
    AudioComponent* audioComponent = nullptr;
    CameraComponent* cameraComponent = nullptr;
    InputComponent* inputComponent = nullptr;
    uint32_t netId = 0;
};