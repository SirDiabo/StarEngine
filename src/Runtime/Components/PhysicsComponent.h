#pragma once
#include "SceneComponent.h"
#include "Runtime/CollisionTrace.h"
#include <vector>

class PhysicsComponent : public SceneComponent {
public:
    bool simulatePhysics = true;
    bool useGravity = true;
    float gravity = 980.0f;
    float gravityScale = 1.0f;
    float mass = 1.0f;
    float linearDrag = 0.0f;
    float groundFriction = 0.0f;
    float maxFallSpeed = 0.0f;
    float maxSlopeAngle = 50.0f;
    float groundSnapDistance = 20.0f;
    float skinWidth = 0.5f;
    float groundSnapSmoothing = 0.4f;
    float bounciness = 0.0f;

    Vector3 velocity = {0.0f, 0.0f, 0.0f};
    bool grounded = false;
    Vector3 groundNormal = {0.0f, 1.0f, 0.0f};

    const char* GetClassName() const override { return "PhysicsComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;

    void SetVelocity(const Vector3& newVelocity);
    void AddImpulse(const Vector3& impulse);

private:
    Vector3 Depenetrate(CollisionTrace::Shape& shape, const CollisionTrace::QueryParams& params, float dtForDepenetrate);
    Vector3 MoveAndSlide(CollisionTrace::Shape& shape, const Vector3& delta, const CollisionTrace::QueryParams& params);
    Vector3 UpdateGround(CollisionTrace::Shape& shape, const CollisionTrace::QueryParams& params, bool wasGrounded, float dtForGround);

    std::vector<CollisionTrace::HitResult> scratchHits;
};

void RegisterPhysicsComponentNodes();
