#pragma once
#include "Object.h"
#include "GraphBehavior.h"
#include "Components/MeshComponent.h"
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

    Vector3 position{ 0, 0, 0 };
    Vector3 rotation{ 0, 0, 0 };
    Vector3 scale{ 1, 1, 1 };

    bool hasBegunPlay = false;

    void AddComponent(Component* component);
    void RemoveComponent(Component* component);
    Component* GetRootComponent();
    MeshComponent* GetMeshComponent() const { return meshComponent; }

protected:
    virtual void OnBeginPlay() {}
    virtual void OnTick(float deltaTime) {}
    virtual void OnEndPlay() {}

private:
    GraphBehavior* graphBehavior = nullptr;
    std::vector<Component*> components;
    Component* rootComponent = nullptr;
    MeshComponent* meshComponent = nullptr;
};