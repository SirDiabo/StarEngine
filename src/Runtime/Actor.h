#pragma once
#include "Object.h"
#include "GraphBehavior.h"
#include "Components/MeshComponent.h"
#include "Components/CollisionComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CameraComponent.h"
#include "raylib.h"
#include <vector>

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
    Matrix GetWorldMatrix() const;

    static Vector3 ComputeWorldPosition(Vector3 localPos, Vector3 parentPos, Vector3 parentRot, Vector3 parentScale);
    static Vector3 ComputeWorldRotation(Vector3 localRot, Vector3 parentRot);
    static Vector3 ComputeWorldScale(Vector3 localScale, Vector3 parentScale);

    Vector3 position{ 0, 0, 0 };
    Vector3 rotation{ 0, 0, 0 };
    Vector3 scale{ 1, 1, 1 };

    bool showMouseCursor = true;

    bool hasBegunPlay = false;

    void AddComponent(Component* component);
    void RemoveComponent(Component* component);
    Component* GetRootComponent();
    const std::vector<Component*>& GetComponents() const { return components; }
    Component* FindComponentByName(const std::string& name) const;
    MeshComponent* GetMeshComponent() const { return meshComponent; }
    CollisionComponent* GetCollisionComponent() const { return collisionComponent; }
    AudioComponent* GetAudioComponent() const { return audioComponent; }
    CameraComponent* GetCameraComponent() const { return cameraComponent; }

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
};