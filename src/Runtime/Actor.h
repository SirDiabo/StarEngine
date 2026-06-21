#pragma once
#include "Object.h"
#include "GraphBehavior.h"
#include "raylib.h"

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

protected:
    virtual void OnBeginPlay() {}
    virtual void OnTick(float deltaTime) {}
    virtual void OnEndPlay() {}

private:
    GraphBehavior* graphBehavior = nullptr;
};
